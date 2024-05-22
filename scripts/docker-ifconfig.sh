#!/bin/bash
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

#Check if docker command exists
if ! hash docker 2>/dev/null; then
    echo "Docker command not found. Please install Docker"
    exit 1
fi

if [ "$(uname)" == "Darwin" ]; then
	flags="E"
else
	flags="P"
fi

file="ifconfig.txt"

if [ -f $file ]; then
    echo "ifconfig file exists... Deleting File"
    rm $file
    printf "${GREEN}Deleted${NC}\n"
fi

#Command to get just the names of all the servers setup in docker
OUTPUT="$(docker ps --format '{{.Names}}' | grep -${flags} "c[\d]*|s[\d]*|sp[\d]*|rp[\d]*" | sort -V)"
#echo "${OUTPUT}"
IPC=""
echo "Server sequence --> IP"
CLIENTS=""
for server in ${OUTPUT}; do
    #Get just the IP Address of all the servers
    IPC="$(docker inspect $server | grep -${flags}o '"IPAddress": *"([0-9]{1,3}[\.]){3}[0-9]{1,3}"'| grep -${flags}o "([0-9]{1,3}[\.]){3}[0-9]{1,3}")"

    # Check if the container is a client
    if [[ "$server" =~ ^c[0-9]+ ]]; then
        if [ -z "$CLIENTS" ]; then
            CLIENTS="${IP}"
        else
            CLIENTS="${CLIENTS}\n${IP}"
        fi
        echo "$server --> ${IP}"
    # Check if the container is a send proxy
    elif [[ "$server" =~ ^sp[0-9]+ ]]; then
        if [ -z "$SEND_PROXIES" ]; then
            SEND_PROXIES="${IP}"
        else
            SEND_PROXIES="${SEND_PROXIES}\n${IP}"
        fi
        echo "$server --> ${IP}"
    # Check if the container is a recv proxy
    elif [[ "$server" =~ ^rp[0-9]+ ]]; then
        if [ -z "$RECV_PROXIES" ]; then
            RECV_PROXIES="${IP}"
        else
            RECV_PROXIES="${RECV_PROXIES}\n${IP}"
        fi
        echo "$server --> ${IP}"
    else
        echo "$server --> ${IP}"
        # Append server IPs to ifconfig.txt
        echo "${IP}" >>$file
    fi
done
echo "Put Client IP at the bottom"
echo -e $CLIENTS >>$file
echo "Send Proxies:"
echo -e $SEND_PROXIES >>$file
echo "Recv Proxies:"
echo -e $RECV_PROXIES >>$file
printf "${GREEN}$file Created!${NC}\n"
