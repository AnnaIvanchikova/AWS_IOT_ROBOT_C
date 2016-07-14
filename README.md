# AWS_IOT_ROBOT_C
AWS IOT Robot: Connecting to the AWS IoT MQTT platform

```
AWS_IoT_Client client;
rc = aws_iot_mqtt_init(&client, &iotInitParams);
rc = aws_iot_mqtt_connect(&client, &iotConnectParams);
```


Subscribe to a topic

```
AWS_IoT_Client client;
rc = aws_iot_mqtt_subscribe(&client, "sdkTest/sub", 11, QOS0, iot_subscribe_callback_handler, NULL);
```


Update Thing Shadow from a device

``` 
rc = aws_iot_shadow_update(&mqttClient, AWS_IOT_MY_THING_NAME, pJsonDocumentBuffer, ShadowUpdateStatusCallback,
                            pCallbackContext, TIMEOUT_4SEC, persistenSubscription);
```
###Config
Need a config file to be started. Content:
- topic to publish (sent messages)
- topic to subscribe (get messages)
- QOS (0 | 1)
- field size (width, height)
- field (matrix)
- initial direction (STRAIGHT = 0, RIGHT = 1, BACK = 2, LEFT = 3)


