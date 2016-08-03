
all:
	gcc picontrol-midirelay.c -lasound -lwiringPi -lwiringPiPca9685 -g -Wall -o picontrol-midirelay

inst: all
	sudo /etc/init.d/picontrol-midirelay stop
	sudo cp picontrol-midirelay /usr/local/bin
	sudo chmod 700 /usr/local/bin/picontrol-midirelay
	sudo chown root /usr/local/bin/picontrol-midirelay
	sudo /etc/init.d/picontrol-midirelay start
