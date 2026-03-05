cURL commands to test GStreamer plugin

curl -d '{"jsonrpc":"2.0","id":"3","method":"Controller.1.activate","params":{"callsign":"org.rdk.GStreamer"}}' http://127.0.0.1:9998/jsonrpc

curl -d '{"jsonrpc":"2.0","id":"3","method":"Controller.1.status@org.rdk.GStreamer"}' http://127.0.0.1:9998/jsonrpc

curl -d '{"jsonrpc":"2.0","id":"3","method":"org.rdk.GStreamer.1.initialize"}' http://127.0.0.1:9998/jsonrpc

curl -d '{"jsonrpc":"2.0","id":"3","method":"org.rdk.GStreamer.1.play"}' http://127.0.0.1:9998/jsonrpc

curl -d '{"jsonrpc":"2.0","id":"3","method":"org.rdk.GStreamer.1.pause"}' http://127.0.0.1:9998/jsonrpc

curl -d '{"jsonrpc":"2.0","id":"3","method":"org.rdk.GStreamer.1.quit"}' http://127.0.0.1:9998/jsonrpc
