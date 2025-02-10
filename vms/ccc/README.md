Add `vagrant plugin install vagrant-env`!

## Installation

Install poetry:
```
python3 -m pip install --upgrade poetry
```

Install dependencies:
```
poetry install
```

Generate gRPC/protobuf code:
```
poetry run python3 -m grpc_tools.protoc -Iproto/ --python_out=. --grpc_python_out=. --pyi_out=. proto/xluuv_ccc/messages.proto
```

Install the package:
```
poetry build
python3 -m pip install dist/*.whl
```

## Running the CCC

Start the server:
```
ccc-server [-h]
```

Start a client:
```
ccc-gui
```

## Running Tests

Tests can be run via
```
poetry run pytest
```
