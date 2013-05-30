#!/bin/bash
#PBS -d /extras/camara/ratcom
#PBS -q batch
#PBS -N ratcomComplete
#PBS -o /extras/camara/ratcom/logs/ratcom.out
#PBS -j oe
#PBS -m abe
#PBS -l nodes=cow3

LOG_DIR="logs"
CONTIKI_LOG="contiki_logs"
NS_3_PATH=/home/kentux/mercurial/ns-3-dev-main
cd $NS_3_PATH
export LD_LIBRARY_PATH=$NS_3_PATH/build
mkdir $LOG_DIR
echo "mkdir $LOG_DIR"
mkdir $CONTIKI_LOG
echo "mkdir $CONTIKI_LOG"

iteracoes=0
beginScript=`date`

echo "Starting script....."
echo 
# intializes the random seeds if we use the same for all 
# the experiments, and use the same number of times,
# the scenarios will be the same
RANDOM=7724 
echo -----------------------------------

seconds=3600 # Max number of rounds to perform per simulation

echo -----------------------------------



echo -----------------------------------
echo " number of nodes" 
echo -----------------------------------
for s in 2 5 10 20 50 100 150 200 250 300 350 400 500
   do
     APPS=udp-server.so

     for ((i=0; $i< $s; i+=1)) ; do
       APPS=$APPS,udp-client.so
     done


echo "Running for $s nodes"

for ((m=0; $m< 32; m+=1)) ; do
    beginIt=`date`

        echo "Round $m of  32 -- started $beginIt"
        /usr/bin/time -a -o $LOG_DIR/log$s.log -f '%E %e %U %P  %M %K %D %W %c %w %I %O %k %x' $NS_3_PATH/build/scratch/contiki-device-example --nNodes=$s --sTime=$seconds --apps=$APPS
	
      rm -rf /dev/shm/ns* /dev/shm/sem.ns*
      mv radio_receive.log $CONTIKI_LOG/radio_receive.$s.$m.log
      mv radio_send.log $CONTIKI_LOG/radio_send.$s.$m.log

 

 done
     echo "m $m started :  $mbeginIt "
    echo "m $m ended : " `date` ;
    echo "---- End m ----" 
  done
echo "Script started :  $beginScript "
echo "Script ended : " `date` ;
echo "---- End ----" 
echo "iteracoes $iteracoes" 
