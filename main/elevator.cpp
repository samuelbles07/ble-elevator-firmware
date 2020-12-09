#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <GPIO.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <Stepper.h>

#include <esp_log.h>
#include <string>
#include <sys/time.h>
#include <sstream>

#include "sdkconfig.h"

#define DEBUG_APP 1


// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define MAX_FLOOR 4

static char LOG_TAG[] = "ElevatorApp";
static int currentFloor = 1;
//---------------Floor position: 0, 1, 2, 3, 4, 5, 6, 7 // 0 change to G for display
static int chosenFloorList[8] = {0, 0, 1, 0, 0, 0, 0, 0};

BLECharacteristic *pCharacteristic;
BLEAdvertising *pAdvertising;

static int convertChosenFloor() {
    int decimal = 0;
    for (int i = 7; i >= 0; i--) {
        decimal = (decimal * 2) + chosenFloorList[i];
    }

    return decimal;
}

class MyNotifyTask: public Task {
	void run(void *data) {
		uint8_t value[2];
		while(1) {
			delay(600);
			value[0] = currentFloor;
			value[1] = convertChosenFloor();
#if DEBUG_APP == 1
			// ESP_LOGI(LOG_TAG, "Current floor: %d | chosen: 0x%.2x", value[0], value[1]);
#endif
			pCharacteristic->setValue(value, 2);
			pCharacteristic->notify();
		} // While 1
	} // run
}; // MyNotifyTask
MyNotifyTask *pMyNotifyTask;

class MainTask: public Task {
	typedef enum {
		GOING_UP,
		GOING_DOWN
	} FloorDirection_t;

	int getDestinationFloor(FloorDirection_t &dir) {
		int result = -1;
		if (dir == GOING_UP) {
			for (int i = currentFloor+1; i < MAX_FLOOR; i++) {
				if (chosenFloorList[i]) { // chosen 
					result = i;
					break;
				}
			}
			if (result == -1) {
				dir = GOING_DOWN;
			}
		}

		if (dir == GOING_DOWN) { 
			for (int i = currentFloor; i >= 0; i--) {
				if (chosenFloorList[i]) { // chosen 
					result = i;
					break;
				}
			}
			if (result == -1) {
				dir = GOING_UP;
			}
		}

		return result;
	}

	void deleteChosenFloor(int floor) {
		chosenFloorList[floor] = 0;
	}

	bool elevatorDetected(int floor) {
		gpio_num_t pin;
		switch (floor) {
			case 0:
				pin = GPIO_NUM_23;
				break;
			case 1:
				pin = GPIO_NUM_32;
				break;
			case 2:
				pin = GPIO_NUM_35;
				break;
			case 3:
				pin = GPIO_NUM_34;
				break;
			default:
				pin = GPIO_NUM_23;
		}

		// Active low
		return !(ESP32CPP::GPIO::read(pin));
	}

	void run(void *data) {
		// Init IR sensor
		ESP32CPP::GPIO::setInput(GPIO_NUM_23);
		ESP32CPP::GPIO::setInput(GPIO_NUM_32);
		ESP32CPP::GPIO::setInput(GPIO_NUM_35);
		ESP32CPP::GPIO::setInput(GPIO_NUM_34);
		
		// Init stepper motor
		Stepper *myStepper_ = nullptr;
		int stepsPerRevolution = 4095;
		myStepper_ = new Stepper(stepsPerRevolution, GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33);
		myStepper_->setSpeed(12);
		myStepper_->stop();

		FloorDirection_t currentDirection = GOING_UP;
		bool isMoving = false;
		int destinationFloor = 0;
		int nextFloor = 0;

		while(1) {
			if (!isMoving) {
				destinationFloor = getDestinationFloor(currentDirection);
				ESP_LOGI(LOG_TAG, "destinationFloor: %d", destinationFloor);
				isMoving = true;
			}

			if (destinationFloor == -1) {
				isMoving = false;
				ESP_LOGI(LOG_TAG, "No chosen floor pick, wait for 1s");
				delay(1000);
				continue;
			}

			if (isMoving) {
				int sensorToDetect;
				if (currentDirection == GOING_UP) {
					nextFloor = currentFloor + 1;
					sensorToDetect = destinationFloor;
				} else {
					nextFloor = currentFloor - 1;
					sensorToDetect = destinationFloor - 1;
				}

				if (elevatorDetected(sensorToDetect)) {

					if (destinationFloor == 0) {
						// Ngakalin ga ada sensor untuk turun ke lantai G / 0
						myStepper_->step(stepsPerRevolution * -1); 
						myStepper_->stop();
						delay(100);
						myStepper_->step(stepsPerRevolution * -1); 
					}

					ESP_LOGI(LOG_TAG, "Stopping motor...");
					myStepper_->stop();
					currentFloor = destinationFloor;
					deleteChosenFloor(destinationFloor);
					isMoving = false;
					
					delay(2500); // Set delay simulation
					continue;
				}

				if (elevatorDetected(nextFloor)) {
					currentFloor = nextFloor;
					ESP_LOGI(LOG_TAG, "Current floor %d", currentFloor);
				}

			}

			// Here move motor
			int stepsDir = (currentDirection == GOING_UP) ? 1 : -1;
			myStepper_->step(10 * stepsDir);

			// dummyCounter++;
			delay(10);
		}
	}
};

class MyCallbacks: public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic *pCharacteristic) {
		std::string value = pCharacteristic->getValue();
		if (value.length() > 0) {
				int tmp = value[0];
				chosenFloorList[tmp] = 1;
				ESP_LOGI(LOG_TAG, "Floor %d added to list", tmp);
			}
		}
};

class MyServerCallbacks: public BLEServerCallbacks {
	void onConnect(BLEServer* pServer) {
		pMyNotifyTask->start();
		pAdvertising->start();
	};

	void onDisconnect(BLEServer* pServer) {
		pMyNotifyTask->stop();
	}
};

void RunElevator() {
	ESP_LOGI(LOG_TAG, "Run Elevator");
	GeneralUtils::dumpInfo();

	pMyNotifyTask = new MyNotifyTask();
	pMyNotifyTask->setStackSize(8000);

	MainTask *pMainTask = new MainTask();
	pMainTask->setStackSize(5000);
	pMainTask->start();

	BLEDevice::init("Elevator");
	BLEDevice::setPower(ESP_PWR_LVL_P1);
	BLEServer *pServer = BLEDevice::createServer();
	pServer->setCallbacks(new MyServerCallbacks());

	BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

	pCharacteristic = pService->createCharacteristic(
		BLEUUID(CHARACTERISTIC_UUID),
		BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
	);

	pCharacteristic->setCallbacks(new MyCallbacks());

	BLE2902* p2902Descriptor = new BLE2902();
	p2902Descriptor->setNotifications(true);
	pCharacteristic->addDescriptor(p2902Descriptor);

	pService->start();

	pAdvertising = pServer->getAdvertising();
	pAdvertising->addServiceUUID(BLEUUID(pService->getUUID()));
	pAdvertising->start();
	
}