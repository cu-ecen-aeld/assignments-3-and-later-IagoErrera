#!/bin/sh

case "$1" in
	start)
		echo "Starting aesdsocket"
		start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
		;;

	stop)
		echo "Stoping aesdsocket"
		start-stop-daemon -K --signal TERM -n aesdsocket
		;;
	*)
		echo "Usage: $0 {start|stop}"
		exit 1
esac

exit 0
