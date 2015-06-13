
all:
	gcc picontrol-midirelay.c -lasound -lwiringPi -g -Wall -o picontrol-midirelay

inst: all
	sudo cp picontrol-midirelay /usr/local/bin
	sudo chmod 700 /usr/local/bin/picontrol-midirelay
	sudo chown root /usr/local/bin/picontrol-midirelay


