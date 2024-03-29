#!/bin/bash
USERNAME=ubuntu
home_directory="/home/ubuntu"
nodes=$1
ifconfig=$2
input="./ifconfig.txt"
i=0
while IFS= read -r line
do

	if_cmd="scp ifconfig.txt ${USERNAME}@${line}:${home_directory}/resilientdb/"
	if [ "$i" -lt "$nodes" ];then
		cmd="scp rundb ${USERNAME}@${line}:${home_directory}/resilientdb/"
	else
		cmd="scp runcl ${USERNAME}@${line}:${home_directory}/resilientdb/"
	fi

	monitor="scp monitorResults.sh ${USERNAME}@${line}:${home_directory}/resilientdb/"
	
	if [ "$ifconfig" -eq 1 ];then
		# echo "$if_cmd"
		$($if_cmd) &
	fi
	$($monitor) &
	# echo "$cmd"
	$($cmd) &
	i=$(($i+1))
done < "$input"
wait