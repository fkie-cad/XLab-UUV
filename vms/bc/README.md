# BridgeCommand-XLUUV

This fork of BridgeCommand ([https://www.bridgecommand.co.uk/](https://www.bridgecommand.co.uk/)) implements support for control via the [bc-proxy](../../vms/utils/dds/README.md) BC-to-DDS middleware.

## How to use BC in VM provisioned with Vagrant

For this the following dependencies are required on the host machine:

- Virtualbox
- Vagrant
- Ansible

For instructions on how to install them please refer to their documentations.

To provision and start the VM, first navigate to the directory that contains the `Vagrantfile` and the `ansible-playbook.yml` and then run the following command:
```
vagrant up --provision
```
This downloads the VM image, sets up the VM in virtualbox and installs all required dependencies and BC in the VM.

Executing the command for the first time can take some time because of the download and installation of desktop environment.
During the setup the VM may reboot a few times.
After the setup is finished the vagrant command should terminate and the VM should be up and running.

Access the VM with X11 forwarding via:
```
ssh -p 2002 -X -i ./.vagrant/machines/bridgecommand/virtualbox/private_key vagrant@localhost
```

The bridgecommand binaries are located in the home directory of this user under `/home/vagrant/bc/bin/`.

To shutdown the VM gracefully use the follwoing command in the host machines terminal:
```
vagrant halt
```

The VM can be deleted completely using:
```
vagrant destroy
```

## Building BridgeCommand manually

### Compiling BridgeCommand

For development purposes or to run BridgeCommand natively one can also build it from source on a host. On a Debian-based distribution the following prerequisites are required:

```
sudo apt install g++ cmake mesa-common-dev libxxf86vm-dev freeglut3-dev libxext-dev libxcursor-dev portaudio19-dev libsndfile1-dev libboost-serialization-dev
```

The project can then be built and ran:

```
cd bc/bin
cmake ../src
make -j $(nproc)
./bridgecommand
```

## Settings

Settings are read from the `bin/bc5.ini` file, unless the file is also present in the BridgeCommand user directory (e.g. `~/.BridgeCommand/x.x/bc5.ini` on Linux), in which case the user directory version takes precedence. Deleting this file should ensure that on the first run the settings will be repopulated by what is currently in the `bin/` directory of the git repository.

The BridgeCommand launcher contains a `Settings Main` button which opens an editor for the `.ini` file, but it can also be edited manually.

This fork introduces five new settings (found under the `DDS Proxy` tab in the GUI):

- `SensorProxyAddress` (default: `192.168.1.1`): the address on which `bc-sen-proxy` is listening for sensor reports
- `SensorProxyPort` (default: `10112`): the port on which `bc-sen-proxy` is listening for sensor reports
- `AivdmProxyAddress` (default: `192.168.1.1`): the address on which `bc-sen-proxy` is listening for ais messages
- `AivdmProxyPort`= (default: `10114`): the port on which `bc-sen-proxy` is listening for ais messages
- `ActuatorProxyPort` (default: `10113`): the port on which BC should listen for actuator commands

## Command line Arguments

Vanilla BridgeCommand arguments:
- `-c`, `--config`: use the passed file as bc5.ini

BridgeCommand-XLUUV:

- `-s`, `--scenario`: skip the scenario choice dialog, start the passed scenario directly, e.g. `d) Leaving Harbour` or `XLUUV-dev`
- `-a`, `--autostart`: skip the pause dialog at scenario start