# SKU_RM0004
The project supports running on RaspberryPi, Ubuntu, [HomeAssistant](https://github.com/UCTRONICS/UCTRONICS_RM0004_HA),You can also use Python to call compiled DLLs on these platforms.
# RaspberryPi

## Deployment service
>  Clone SKU_RM0004 library 
```bash
git clone https://github.com/UCTRONICS/SKU_RM0004.git
```
> Compile 
```bash
cd SKU_RM0004
make clean && make 
```
## Add automatic start script
```bash
./deployment_service.sh   
```
**reboot your system**
```bash
sudo reboot
```
## Show your own message

`display-cli` replaces the top line of the display with a message of your
choosing, while the CPU, RAM, temperature and disk readings carry on as
usual. The running display picks up a change within a couple of seconds;
there is nothing to rebuild or restart.

```bash
display-cli "Backup server"     # show a message
display-cli --show              # print what is currently set
display-cli --clear             # go back to showing the IP address
```

The top line fits 19 characters. Longer messages, and anything outside
printable ASCII, are trimmed to what the display is able to draw.

The message is kept in `/var/lib/uctronics-display/message` and survives a
reboot. `deployment_service.sh` makes that directory yours, so `display-cli`
does not need `sudo`. Set `UCTRONICS_DISPLAY_MESSAGE_FILE` to use a
different file.

## How to uninstall the uctronics-display.service

```bash
sudo systemctl disable uctronics-display.service
sudo rm /etc/systemd/system/uctronics-display.service
sudo rm -f /usr/local/bin/display-cli
sudo rm -rf /var/lib/uctronics-display
sudo systemctl daemon-reload
```
## How to use NVMe 
***Note: only for Raspberry Pi 5 and UC-B86 NVMe hat***

https://github.com/UCTRONICS/SKU_RM0004/blob/main/data/NVMe_User_Guide.md





