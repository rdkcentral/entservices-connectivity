# Calculator Plugin

## Overview
The Calculator plugin provides basic arithmetic operations (add, sub, mul, div, mod) via JSON-RPC, modeled after the Bluetooth plugin structure.

## Methods
- add
- sub
- mul
- div
- mod

## Example Usage
```
curl --header "Content-Type: application/json" --request POST --data '{"jsonrpc":"2.0", "id":"3", "method":"org.rdk.Calculator.1.add", "params":{"a":5,"b":3}}' http://127.0.0.1:9998/jsonrpc
```

## Configuration
See Calculator.conf.in for configuration options.
