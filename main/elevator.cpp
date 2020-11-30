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
#define MAX_FLOOR 8

static char LOG_TAG[] = "ElevatorApp";
static int currentFloor = 1;
//---------------Floor position: 0, 1, 2, 3, 4, 5, 6, 7 // 0 change to G for display
static int chosenFloorList[8] = {1, 0, 1, 0, 1, 0, 1, 1};

BLECharacteristic *pCharacteristic;

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
			delay(300);
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

	void initMotor(Stepper *stepper) {
		int stepsPerRevolution = 4095;
		gpio_num_t pin_1 = GPIO_NUM_27;
		gpio_num_t pin_2 = GPIO_NUM_26;
		gpio_num_t pin_3 = GPIO_NUM_25;
		gpio_num_t pin_4 = GPIO_NUM_33;
		stepper = new Stepper(stepsPerRevolution, pin_1, pin_2, pin_3, pin_4);
		stepper->setSpeed(10);
	}

	int getNextFloor(FloorDirection_t &dir) {
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

	void run(void *data) {
		ESP32CPP::GPIO::setInput(GPIO_NUM_5);
		
		Stepper *myStepper_ = nullptr;
		int stepsPerRevolution = 4095;
		gpio_num_t pin_1 = GPIO_NUM_27;
		gpio_num_t pin_2 = GPIO_NUM_26;
		gpio_num_t pin_3 = GPIO_NUM_25;
		gpio_num_t pin_4 = GPIO_NUM_33;
		myStepper_ = new Stepper(stepsPerRevolution, pin_1, pin_2, pin_3, pin_4);
		myStepper_->setSpeed(10);
		myStepper_->stop();

		FloorDirection_t currentDirection = GOING_UP;
		bool isMoving = false;
		int nextFloor = 0;

		while(1) {
			if (!isMoving) {
				nextFloor = getNextFloor(currentDirection);
				ESP_LOGI(LOG_TAG, "Nextfloor: %d", nextFloor);
				isMoving = true;
			}

			if (nextFloor == -1) {
				isMoving = false;
				ESP_LOGI(LOG_TAG, "No chosen floor pick, wait for 1s");
				delay(1000);
				continue;
			}

			// IO active low
			bool ioState = ESP32CPP::GPIO::read(GPIO_NUM_5);
			if (!ioState && isMoving) {
				ESP_LOGI(LOG_TAG, "Stopping motor...");
				myStepper_->stop();
				currentFloor = nextFloor;
				deleteChosenFloor(nextFloor);

				isMoving = false;
				
				delay(2500); // Set delay simulation
				continue;
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
	BLEServer *pServer = BLEDevice::createServer();
	pServer->setCallbacks(new MyServerCallbacks());

	BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));

	pCharacteristic = pService->createCharacteristic(
		BLEUUID(CHARACTERISTIC_UUID),
		BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
	);

	pCharacteristic->setCallbacks(new MyCallbacks());
	pCharacteristic->addDescriptor(new BLE2902());

	pService->start();

	BLEAdvertising *pAdvertising = pServer->getAdvertising();
	pAdvertising->start();
	
}