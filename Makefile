
all:
	gcc picontrol-midirelay.c -lasound -lwiringPi -g -Wall -o picontrol-midirelay

inst: all
	sudo cp picontrol-midirelay /usr/sbin
	sudo chmod 700 /usr/sbin/picontrol-midirelay
	sudo chown root /usr/sbin/picontrol-midirelay


