-----------------
# bARTONmATTER

## Versions
`org.rdk.barton.1`

## Properties:
```
## Methods:
```
TO activate the plugin
curl -H "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.activate", "params":{"callsign":"org.rdk.barton"}}' http://127.0.0.1:9998/jsonrpc


Destory the application
curl -H "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0","id":"3","method": "Controller.1.deactivate", "params":{"callsign":"org.rdk.barton"}}' http://127.0.0.1:9998/jsonrpc

