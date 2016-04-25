#include "iec61850_server.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include "sv_subscriber.h"
#include "hal_thread.h"


/* Include the generated header with the model access handles */
#include "static_model.h"

/* import IEC 61850 device model created from SCL-File */
extern IedModel iedModel;

static int running = 0;

void sigint_handler(int signalId)
{
    running = 0;
}

static bool runningsmv = true;

void sigint_handlersmv(int signalId)
{
    runningsmv = 0;
}

int i;

int smpcnt, indice;

int argc;
   
int ret1, ret2;

char** argv;

float cA[80] = {0} , cB[80] = {0} , cC[80] = {0} , nC, vA[80] = {0}, vB[80] = {0}, vC[80] = {0}, nV;

float activePower = 0, pA = 0, pB = 0, pC = 0;

SVReceiver receiver;

IedServer iedServer;

pthread_t thread;

/* Callback handler for received SV messages */
static void
svUpdateListener (SVSubscriber subscriber, void* parameter, SVClientASDU asdu)
{
    printf("svUpdateListener called\n");

    const char* svID = SVClientASDU_getSvId(asdu);

    if (svID != NULL)
        printf("  svID=(%s)\n", svID);
	smpcnt = SVClientASDU_getSmpCnt(asdu);
	printf("  smpCnt: %i\n", smpcnt);
    printf("  confRev: %u\n", SVClientASDU_getConfRev(asdu));
    
	indice = smpcnt - 80* ((smpcnt-1)/80)-1;
	

	if (SVClientASDU_getDataSize(asdu) >= 8) 
	{    	
		cA[indice] = SVClientASDU_getFLOAT32(asdu, 0);
		cB[indice] = SVClientASDU_getFLOAT32(asdu, 4);
		cC[indice] = SVClientASDU_getFLOAT32(asdu, 8);
		nC = SVClientASDU_getFLOAT32(asdu, 12);
		vA[indice] = SVClientASDU_getFLOAT32(asdu, 16);
		vB[indice] = SVClientASDU_getFLOAT32(asdu, 20);
		vC[indice] = SVClientASDU_getFLOAT32(asdu, 24);
		nV = SVClientASDU_getFLOAT32(asdu, 28);
		printf("   Current_PhaseA[%d]: %f\n", indice, cA[indice]);
        //printf("   Current_PhaseB: %f\n", cB[indice] );
        //printf("   Current_PhaseC: %f\n", cC[indice] );
        //printf("   Neutral_Current: %f\n", nC);
        //printf("   Voltage_PhaseA: %f\n", vA[indice] );
        //printf("   Voltage_PhaseB: %f\n", vB[indice] );
        //printf("   Voltage_PhaseC: %f\n", vC[smpcnt-1] );
        //printf("   Neutral_To_Ground_Voltage: %f\n", nV);       
	}
	
	else 
	{
		printf("No power");
	}
  
}

void *create_sv_subsciber()
{
	
	receiver = SVReceiver_create();

    if (argc > 1) 
    {
        SVReceiver_setInterfaceId(receiver, argv[1]);
		printf("Set interface id: %s\n", argv[1]);
    }
    
    else 
    {  
        printf("Using interface eth1\n");
     
        SVReceiver_setInterfaceId(receiver, "eth1");
    }
    /* Create a subscriber listening to SV messages with APPID 4000h */
    SVSubscriber subscriber = SVSubscriber_create(NULL, 0x4000);
  
    /* Install a callback handler for the subscriber */
    SVSubscriber_setListener(subscriber, svUpdateListener, NULL);

    /* Connect the subscriber to the receiver */
    SVReceiver_addSubscriber(receiver, subscriber);
 
    /* Start listening to SV messages - starts a new receiver background thread */
    SVReceiver_start(receiver);
    
	pthread_exit(NULL); 
	

}

void *create_ied_server()
{
	iedServer = IedServer_create(&iedModel);

    // TODO get stored values from persistent storage

    // TODO set initial measurement and status values from process

    /* MMS server will be instructed to start listening to client connections. */
    IedServer_start(iedServer, 102);

    if (!IedServer_isRunning(iedServer))
	{
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(iedServer);
        exit(-1);
    }
	

		
    running = 1;
    signal(SIGINT, sigint_handler);
    
   while (running) 
    {
		pA = 0; 
		pB = 0; 
		pC = 0; 
		activePower = 0;
		
		for (i=0; i<80; i++)
		{   
			printf("cA[%d] = %f\n", i, cA[i]); 
			pA = pA + cA[i]*vA[i];
			pB = pB + cB[i]*vB[i];
			pC = pC + cC[i]*vC[i];
			activePower =activePower + cA[i]*vA[i] + cB[i]*vB[i] + cC[i]*vC[i];	
		}
		printf("**************************----------------------------**********************\n");
		activePower = (float)(activePower/80);
		pA = (float)pA/80;	
		pB = (float)pB/80;
		pC = (float)pC/80;
		 
		   
		MmsValue* pAValue = MmsValue_newFloat(pA);
		MmsValue* pBValue = MmsValue_newFloat(pB);
		MmsValue* pCValue = MmsValue_newFloat(pC);
		MmsValue* powerValue = MmsValue_newFloat(activePower);
		MmsValue* timeval = MmsValue_newUtcTime(time(NULL));
			
		MmsValue_setUtcTime(timeval, time(NULL));
	
		IedServer_lockDataModel(iedServer);

		IedServer_updateAttributeValue(iedServer, IEDMODEL_Inverter_MMXU1_W_phsA_cVal_mag_f, pAValue );
		IedServer_updateAttributeValue(iedServer, IEDMODEL_Inverter_MMXU1_W_phsB_cVal_mag_f, pBValue );
		IedServer_updateAttributeValue(iedServer, IEDMODEL_Inverter_MMXU1_W_phsC_cVal_mag_f, pCValue );   		
		IedServer_updateAttributeValue(iedServer, IEDMODEL_Inverter_MMXU1_TotW_mag_f, powerValue );
		IedServer_updateAttributeValue(iedServer, IEDMODEL_Inverter_MMXU1_TotW_t, timeval);

		IedServer_unlockDataModel(iedServer);
        
		MmsValue_setFloat(powerValue, activePower) ;
    
		Thread_sleep(500);
	}

	//stop MMS server - close TCP server socket and all client sockets //
    IedServer_stop(iedServer);

    // Cleanup - free all resources //
    IedServer_destroy(iedServer);
    
	/* Stop listening to SV messages */
    SVReceiver_stop(receiver);
    /* Cleanup and free resources */
    SVReceiver_destroy(receiver);
}
int 
main(int argc, char** argv)
{
	

    // make threads
    ret1 = pthread_create(&thread, NULL, create_sv_subsciber, NULL);
	if (ret1)
	{
         printf("ERROR; return code from pthread_create() is %d\n", ret1);
         exit(-1);
	}
	create_ied_server();
	pthread_exit(NULL);
	//pthread_join(thread1,NULL);
	//pthread_join(thread2,NULL);
	//create_sv_subsciber();	
	

} 
