/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file subscribe_publish_sample.c
 * @brief simple MQTT publish and subscribe on the same topic
 *
 * This example takes the parameters from the aws_iot_config.h file and establishes a connection to the AWS IoT MQTT Platform.
 * It subscribes and publishes to the same topic - "sdkTest/sub"
 *
 * If all the certs are correct, you should see the messages received by the application in a loop.
 *
 * The application takes in the certificate path, host name , port and the number of times the publish should happen.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"


/*
* поле
*/
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 300

#define STRAIGHT 0
#define RIGHT 1
#define BACK 2
#define LEFT 3




/**
 * @brief Default cert location
 */
char certDirectory[PATH_MAX + 1] = "../../../certs";

/**
 * @brief Default MQTT HOST URL is pulled from the aws_iot_config.h
 */
char HostAddress[255] = AWS_IOT_MQTT_HOST;


/**
 * @brief Default MQTT port is pulled from the aws_iot_config.h
 */
uint32_t port = AWS_IOT_MQTT_PORT;

/**
 * @brief This parameter will avoid infinite loop of publish and exit the program after certain number of publishes
 */
uint32_t publishCount = 3;

static bool wallDetected = false;
static char* state = "";
static char* direction = "";
static char* position = "";
int i = 1;
int j = 1;
char* message = "";


static bool isStopped = true;
static int goDirection = STRAIGHT;

const int pol[8][9] = { {1, 1, 1, 1, 1, 1, 1, 1, 1},
	      		{1, 0, 0, 0, 0, 0, 0, 0, 1},
	      		{1, 0, 0, 1, 0, 0, 0, 0, 1},
	      		{1, 0, 0, 0, 0, 0, 0, 0, 1},
	      		{1, 0, 0, 1, 0, 0, 0, 0, 1},
	      		{1, 0, 0, 1, 1, 1, 0, 1, 1},
	      		{1, 0, 0, 0, 0, 0, 0, 0, 1},
	      		{1, 1, 1, 1, 1, 1, 1, 1, 1}};

static void setDirection() {
	switch (goDirection) {
		case STRAIGHT:
			strcpy(direction, "go");
			break;
		case RIGHT:
		 	strcpy(direction, "right");
			break;
		case LEFT:
		 	strcpy(direction, "left");
			break;
		case BACK:
		 	strcpy(direction, "back");
			break;	
	}
}

static void setPosition() {
	char str[80];
	char numBuffer[10];
	sprintf(numBuffer, "%d", i);
	strcpy (str,"(");
	strcat (str, numBuffer);
	strcat (str,", ");
	sprintf(numBuffer, "%d", j);
	strcat (str, numBuffer);
	strcat (str,")");
	strcpy(position, str);
}

static void turnRightDirection(){
	if(goDirection == 3)
		goDirection = 0;
	else
		goDirection++;
}

static void turnLeftDirection(){
	if(goDirection == 0)
		goDirection = 3;
	else
		goDirection--;
}

static bool Wall(){
	switch(goDirection){
		case STRAIGHT:
			if(i - 1 >= 0)
				if(pol[i - 1][j] == 1)
					return true;
		break;
		case RIGHT:
			if(j + 1 < 9)
				if(pol[i][j + 1] == 1)
					return true;
		break;	
		case LEFT:
			if(j - 1 >= 0)
				if(pol[i][j - 1] == 1)
					return true;
		break;
		case BACK:
			if(i + 1 < 8)
				if(pol[i + 1][j] == 1)
					return true;
		break;			
	}
	return false;
}


void makeStep(){
	switch (goDirection) {
		case STRAIGHT:
		 	if (i - 1 >= 0)
				i--;
			break;
		case RIGHT:
		 	if (j + 1 < 9)
				j++;
			break;
		case LEFT:
		 	if (j - 1 >= 0)
				j--;
			break;
		case BACK:
		 	if (i + 1 < 8)
				i++;
			break;	
	}
	
}

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
								const char *pReceivedJsonDocument, void *pContextData) {
	IOT_UNUSED(pThingName);
	IOT_UNUSED(action);
	IOT_UNUSED(pReceivedJsonDocument);
	IOT_UNUSED(pContextData);

	if(SHADOW_ACK_TIMEOUT == status) {
		IOT_INFO("Update Timeout--");
	} else if(SHADOW_ACK_REJECTED == status) {
		IOT_INFO("Update RejectedXX");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		//IOT_INFO("Update Accepted !!");
	}
}

void windowActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	IOT_UNUSED(pJsonString);
	IOT_UNUSED(JsonStringDataLen);

	if(pContext != NULL) {
		IOT_INFO("Delta - Window state changed to %d", *(bool *) (pContext->pData));
	}
}


void publish(AWS_IoT_Client client, char* message) {
	IoT_Publish_Message_Params paramsQOS0;
	char cPayload[100];
	IoT_Error_t rc = FAILURE;

	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;
	sprintf(cPayload, "%s", message);
	paramsQOS0.payloadLen = strlen(cPayload);
	rc = aws_iot_mqtt_publish(&client, "Topic_Log", 9, &paramsQOS0);
	if(SUCCESS != rc) {
		IOT_ERROR("Publish Error");
	}
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
									IoT_Publish_Message_Params *params, void *pData) {
	IOT_UNUSED(pData);
	IOT_UNUSED(pClient);
		
	sprintf(message, "%.*s", (int) params->payloadLen, (char*)params->payload);
	wallDetected = false;
	if (strcmp(message, "go") == 0) {
		if (Wall()) {
			wallDetected = true;
			publish(*pClient, "WALL!");		
		} else {
			strcpy(state, "go");
			isStopped = false;
			publish(*pClient, "I am going!");
		}
	} else {
		if (strcmp(message, "left") == 0) {
			if (!isStopped) 
				publish(*pClient, "Stop it before turning left.");
			else {
				//strcpy(state, "left");
				turnLeftDirection();
				publish(*pClient, "I turned left!");
				if (Wall()) {
					wallDetected = true;
					publish(*pClient, "Wall!");
				}	
			}
		} else {
			if (strcmp(message, "right") == 0) {
				if (!isStopped) 
					publish(*pClient, "Stop it before turning right.");
				else {
					//strcpy(state, "right");
					turnRightDirection();
					publish(*pClient, "I turned right!");
					if (Wall()) {
						wallDetected = true;
						publish(*pClient, "Wall!");
					}	
				}
			} else {
				if (strcmp(message, "stop") == 0) {
					strcpy(state, "stop");
					isStopped = true;
					publish(*pClient, "Robot stopped!");
				}
			}
		}
	}
	

	IOT_INFO("> %.*s:\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char*)params->payload);
	IOT_INFO("On Device: robot state: %s", state);
	IOT_INFO("On Device: wall detected: %d", wallDetected);
}


void disconnectCallbackHandler(AWS_IoT_Client *pClient, void *data) {
	IOT_WARN("MQTT Disconnect");
	IoT_Error_t rc = FAILURE;

	if(NULL == pClient) {
		return;
	}

	IOT_UNUSED(data);

	if(aws_iot_is_autoreconnect_enabled(pClient)) {
		IOT_INFO("Auto Reconnect is enabled, Reconnecting attempt will start now");
	} else {
		IOT_WARN("Auto Reconnect not enabled. Starting manual reconnect...");
		rc = aws_iot_mqtt_attempt_reconnect(pClient);
		if(NETWORK_RECONNECTED == rc) {
			IOT_WARN("Manual Reconnect Successful");
		} else {
			IOT_WARN("Manual Reconnect Failed - %d", rc);
		}
	}
}

void wallActuate_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
	IOT_UNUSED(pJsonString);
	IOT_UNUSED(JsonStringDataLen);
	
	if(pContext != NULL) {
		IOT_INFO("Delta - Wall detection state changed to %d", *(bool *) (pContext->pData));
	}
}

void parseInputArgsForConnectParams(int argc, char **argv) {
	int opt;

	while(-1 != (opt = getopt(argc, argv, "h:p:c:x:"))) {
		switch(opt) {
			case 'h':
				strcpy(HostAddress, optarg);
				IOT_DEBUG("Host %s", optarg);
				break;
			case 'p':
				port = atoi(optarg);
				IOT_DEBUG("arg %s", optarg);
				break;
			case 'c':
				strcpy(certDirectory, optarg);
				IOT_DEBUG("cert root directory %s", optarg);
				break;
			case 'x':
				publishCount = atoi(optarg);
				IOT_DEBUG("publish %s times\n", optarg);
				break;
			case '?':
				if(optopt == 'c') {
					IOT_ERROR("Option -%c requires an argument.", optopt);
				} else if(isprint(optopt)) {
					IOT_WARN("Unknown option `-%c'.", optopt);
				} else {
					IOT_WARN("Unknown option character `\\x%x'.", optopt);
				}
				break;
			default:
				IOT_ERROR("Error in command line argument parsing");
				break;
		}
	}

}


int main(int argc, char **argv) {
	bool infinitePublishFlag = true;

	char rootCA[PATH_MAX + 1];
	char clientCRT[PATH_MAX + 1];
	char clientKey[PATH_MAX + 1];
	char CurrentWD[PATH_MAX + 1];
	char cPayload[100];

	int32_t i = 0;

	IoT_Error_t rc = FAILURE;

 	char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
	size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);
	char *pJsonStringToUpdate;

	/*state = "stop";
	message = "stop";
	position = "stop";
	direction = "stop";*/

	state = (char*)malloc(sizeof(char) * 100);
	message = (char*)malloc(sizeof(char) * 100);
	position = (char*)malloc(sizeof(char) * 100);
	direction = (char*)malloc(sizeof(char) * 100);

	jsonStruct_t stateHandler;
	stateHandler.cb = NULL;
	stateHandler.pKey = "state";
	stateHandler.pData = state;
	stateHandler.type = SHADOW_JSON_STRING;

	jsonStruct_t wallDetectedHandler;
	wallDetectedHandler.cb = wallActuate_Callback;
	wallDetectedHandler.pKey = "wallDetected";
	wallDetectedHandler.pData = &wallDetected;
	wallDetectedHandler.type = SHADOW_JSON_BOOL;

	jsonStruct_t directionHandler;
	directionHandler.cb = NULL;
	directionHandler.pKey = "direction";
	directionHandler.pData = direction;
	directionHandler.type = SHADOW_JSON_STRING;

	jsonStruct_t positionHandler;
	positionHandler.cb = NULL;
	positionHandler.pKey = "position";
	positionHandler.pData = position;
	positionHandler.type = SHADOW_JSON_STRING;	



	AWS_IoT_Client client;
	IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
	IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

	IoT_Publish_Message_Params paramsQOS0;
	//IoT_Publish_Message_Params paramsQOS1;

	parseInputArgsForConnectParams(argc, argv);

	IOT_INFO("\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

	getcwd(CurrentWD, sizeof(CurrentWD));
	snprintf(rootCA, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_ROOT_CA_FILENAME);
	snprintf(clientCRT, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_CERTIFICATE_FILENAME);
	snprintf(clientKey, PATH_MAX + 1, "%s/%s/%s", CurrentWD, certDirectory, AWS_IOT_PRIVATE_KEY_FILENAME);

	IOT_DEBUG("rootCA %s", rootCA);
	IOT_DEBUG("clientCRT %s", clientCRT);
	IOT_DEBUG("clientKey %s", clientKey);

	ShadowInitParameters_t sp = ShadowInitParametersDefault;
	sp.pHost = AWS_IOT_MQTT_HOST;
	sp.port = AWS_IOT_MQTT_PORT;
	sp.pClientCRT = clientCRT;
	sp.pClientKey = clientKey;
	sp.pRootCA = rootCA;
	sp.enableAutoReconnect = false;
	sp.disconnectHandler = NULL;

	IOT_INFO("Shadow Init");
	rc = aws_iot_shadow_init(&client, &sp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Connection Error");
		return rc;
	}

	ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
	scp.pMyThingName = AWS_IOT_MY_THING_NAME;
	scp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
	scp.mqttClientIdLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);

	IOT_INFO("Shadow Connect");
	rc = aws_iot_shadow_connect(&client, &scp);
	if(SUCCESS != rc) {
		IOT_ERROR("Shadow Connection Error");
		return rc;
	}

	rc = aws_iot_shadow_set_autoreconnect_status(&client, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}
	
	mqttInitParams.enableAutoReconnect = false; // We enable this later below
	mqttInitParams.pHostURL = HostAddress;
	mqttInitParams.port = port;
	mqttInitParams.pRootCALocation = rootCA;
	mqttInitParams.pDeviceCertLocation = clientCRT;
	mqttInitParams.pDevicePrivateKeyLocation = clientKey;
	mqttInitParams.mqttCommandTimeout_ms = 20000;
	mqttInitParams.tlsHandshakeTimeout_ms = 5000;
	mqttInitParams.isSSLHostnameVerify = true;
	mqttInitParams.disconnectHandler = disconnectCallbackHandler;
	mqttInitParams.disconnectHandlerData = NULL;

	rc = aws_iot_mqtt_init(&client, &mqttInitParams);
	if(SUCCESS != rc) {
		IOT_ERROR("aws_iot_mqtt_init returned error : %d ", rc);
		return rc;
	}

	connectParams.keepAliveIntervalInSec = 10;
	connectParams.isCleanSession = true;
	connectParams.MQTTVersion = MQTT_3_1_1;
	connectParams.pClientID = AWS_IOT_MQTT_CLIENT_ID;
	connectParams.clientIDLen = (uint16_t) strlen(AWS_IOT_MQTT_CLIENT_ID);
	connectParams.isWillMsgPresent = false;

	IOT_INFO("Connecting...");
	rc = aws_iot_mqtt_connect(&client, &connectParams);
	if(SUCCESS != rc) {
		IOT_ERROR("Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
		return rc;
	}
	/*
	 * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
	 *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
	 *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
	 */
	rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
	if(SUCCESS != rc) {
		IOT_ERROR("Unable to set Auto Reconnect to true - %d", rc);
		return rc;
	}

	IOT_INFO("Subscribing...");
	rc = aws_iot_mqtt_subscribe(&client, "MyTopic", 7, QOS0, iot_subscribe_callback_handler, NULL);
	if(SUCCESS != rc) {
		IOT_ERROR("Error subscribing : %d ", rc);
		return rc;
	}

	IOT_INFO("Subscribed.");
	//sprintf(cPayload, "%s : %d ", "hello from SDK", i);

	paramsQOS0.qos = QOS0;
	paramsQOS0.payload = (void *) cPayload;
	paramsQOS0.isRetained = 0;


		
	

	while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)
		  || infinitePublishFlag) {

		//Max time the yield function will wait for read messages
		rc = aws_iot_mqtt_yield(&client, 100);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}

		rc = aws_iot_shadow_yield(&client, 200);
		if(NETWORK_ATTEMPTING_RECONNECT == rc) {
			sleep(1);
			// If the client is attempting to reconnect we will skip the rest of the loop.
			continue;
		}

		//IOT_INFO("-->sleep");
		sleep(1);

		if (!isStopped) {
			if (Wall()) {
				publish(client, "WALL! The robot is stopping");
				strcpy(state, "stop");
				wallDetected = true;
				isStopped = true;
			} else {
				makeStep();
				IOT_INFO("Current position: (%d, %d)", i, j);
			}		
		}

		setDirection();
		setPosition();
		stateHandler.pData = state;
		wallDetectedHandler.pData = &wallDetected;
		directionHandler.pData = direction;
		positionHandler.pData = position;		
		//IOT_INFO("On Device: state: %s", state);


		rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
		if(SUCCESS == rc) {
			rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 3, &stateHandler, &wallDetectedHandler, &directionHandler, &positionHandler);
			if(SUCCESS == rc) {
				rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
				if(SUCCESS == rc) {
					//IOT_INFO("Update Shadow: %s", JsonDocumentBuffer);
					rc = aws_iot_shadow_update(&client, AWS_IOT_MY_THING_NAME, JsonDocumentBuffer,
											   ShadowUpdateStatusCallback, NULL, 4, true);
				}
			}
		}

		

		//sleep(3);
	}

	if(SUCCESS != rc) {
		IOT_ERROR("An error occurred in the loop.\n");
	} else {
		IOT_INFO("Disconnecting");
		rc = aws_iot_shadow_disconnect(&client);

		if(SUCCESS != rc) {
			IOT_ERROR("Disconnect error %d", rc);
		}

		IOT_INFO("Publish done\n");
	}

	return rc;
}
