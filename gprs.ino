

// You must change the buffer size in SoftwareSerial.h
// find this line:
//	#define _SS_MAX_RX_BUFF 64
// and change 64 to 128
	
#include <SoftwareSerial.h>
#include <Time.h>

//#define DEBUG 1

char siteId[4] = "0";
long failCount = 0;

int  modemPower = -1;
char apn[32] = "telargo.t-mobile.com";  // this account is long since expired
char usr[16] = "Emb8b4b";
char pwd[16] = "98CB4385";
int cmdTimeout = 30000;

unsigned long nextBin = 0;
unsigned long nextFlowDetect = 0;
unsigned long prevFlowCount = 0;

char response[256];

const int interval_s = 300; // bin size in seconds
const unsigned long interval = 300000; //in milliseconds
const unsigned long flowInterval = 5000;

volatile unsigned long currentPulseCount;

const int MAX_COUNT = 96;
const int SEND_COUNT = 1;
int pulseCountStartIndex = 0;
int pulseCountEndIndex = 0;
unsigned long pulseCount[MAX_COUNT];

SoftwareSerial gprs(7, 8);


//#######################################################################
// variables created by the build process when compiling the sketch
extern int __bss_end;
extern void *__brkval;

// function to return the amount of free RAM
int memoryFree()
{
  int freeValue;

  if ((int)__brkval == 0)
     freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else
    freeValue = ((int)&freeValue) - ((int)__brkval);

  return freeValue;
}

//#######################################################################
void pulseISR() 
{
  currentPulseCount++;
}

//#######################################################################
int getResponse( int nchars, int waitTime = cmdTimeout )
{
	long endTime = millis() + waitTime;
	int index = 0;
	int retVal = -1;
	
	response[0] = '\0';
	
	while( (index < nchars) && (millis() < endTime))
	{
		if( gprs.available())
		{
			char c = (char)gprs.read();
			
			endTime = millis() + waitTime;
			
			response[index] = c;
			index++;
		}
	}
	
	response[index] = '\0';
	
	if( index == nchars)
	{
		retVal = 0;
	}
		
	#if DEBUG >= 4
		Serial.println( response);
	#endif
	
	return retVal;
}


//#######################################################################
int getResponseUntil( const char* const* targets, int waitTime = cmdTimeout )
{
	long endTime = millis() + waitTime;
	int index = 0;
	int retVal = -1;
	const char* target = 0;
	int targetIndex = 0;
	
	memset( response, 0, sizeof( response));
	
	while( (retVal == -1) && (millis() < endTime))
	{
		if( gprs.available())
		{
			char c = (char)gprs.read();
			endTime = millis() + cmdTimeout;
			
			if( isprint( c) || c == '\r' || c == '\n')
			{
				response[index] = c;
				
				targetIndex = 0;
				target = targets[targetIndex];
				
				while( target)
				{
					if( strstr( response, target) != 0)
					{
						retVal = targetIndex;
						break;
					}
					
					targetIndex++;
					target = targets[targetIndex];
				}
				
				#if DEBUG
					//Serial.print( index);
					//Serial.println( c);
				#endif
				
				index++;
			}
			else
			{
				#if DEBUG
					//Serial.print( F("non printable char "));
					//Serial.println( (int)c);
				#endif
			}
		}
	}
			
	#if DEBUG >= 4
		Serial.println( response);
	#endif
	
	return retVal;
}


//#######################################################################
int getCommandResponse( int waitTime = cmdTimeout)
{
	long endTime = millis() + waitTime;
	int index = 0;
	int lastPos = -1;
	int lastChar = 0;
	int retVal = -1;
	int found = 0;
	
	response[0] = '\0';
	
	while( (found == 0) && (millis() < endTime))
	{
		if( gprs.available())
		{
			char c = (char)gprs.read();
			
			response[index] = c;
			index++;
			endTime = millis() + waitTime;			
			
			switch( c)
			{
				case 'O':	
					lastPos = index; 
					lastChar = c;
					break;
					
				case 'K':	
					if((lastPos != -1) && ((index - lastPos) == 1) && (lastChar == 'O'))
					{				
						retVal = 0;
					}
					break;
					
				case 'R':	
					if((lastPos != -1) && ((index - lastPos) == 1) && (lastChar == 'O'))
					{				
						retVal = 1;
					}
					break;
					
				case 'I':	
					lastPos = index; 
					lastChar = c;
					break;
					
				case 'L':	
					if((lastPos != -1) && ((index - lastPos) == 1) && (lastChar == 'I'))
					{				
						retVal = 1;
					}
					break;
					
				case '\r':	
					// ignore
					break;
					
				case '\n':	
					if((lastPos != -1) && ((index - lastPos) == 3))
					{				
						found = 1;
					}
					else
					{
						lastPos = -1;
						lastChar = 0;
					}
					break;
					
				default:
					lastPos = -1;
					lastChar = 0;
					break;
			}
		}
	}
	
	response[index] = '\0';
	
	if( !found)
	{
		retVal = -1;
	}
		
	#if DEBUG >= 4
		Serial.println( response);
	#endif
	
	return retVal;
}

//#######################################################################
bool syncClock()
{
	// AT+CCLK?
	//
	// +CCLK: "13/09/13,06:54:55-20"
	//
	// OK
	char* p = 0;
	int ts[6];
	int i = 0;
	int error = 0;
	
	#if DEBUG
		Serial.print( F("Sync clock - "));
	#endif
	
	gprs.println( F("AT+CCLK?")); 
	error = getCommandResponse();
	
	if( !error)
	{
		p = strstr( response, "+CCLK: ");
	}
	else
	{
		#if DEBUG
			Serial.print( (error == -1) ? F(" TIMEOUT ") : F(" CMD ERROR "));
		#endif
	}
	
	if( p)
	{
		int tokCount = 0;
		
		p = strtok( p, " ");
		
		while( p && (tokCount < 7))
		{
			#if DEBUG
				//Serial.print( F("token = "));
				//Serial.println(  p);
			#endif
			
			if( isdigit(*p))
			{
				ts[i++] = atoi( p);
			}
			
			p = strtok( NULL, "/,:\"");
			
			tokCount++;
		}
		
		if( tokCount == 7)
		{
			ts[0] += 2000;
			setTime( ts[3], ts[4], ts[5], ts[2], ts[1], ts[0]);
			
			#if DEBUG >= 3
				for( int j = 0; j < 6; j++)
				{
					Serial.print( ts[j]);
					Serial.print( " ");
				}
				Serial.println( now());
			#endif
		}
		else
		{
			error = 1;
			
			#if DEBUG
				Serial.print( F(" wrong token count "));
			#endif
		}
	}
	else
	{
		#if DEBUG
			Serial.print( F(" CCLK not found "));
		#endif
		
		error = 1;
	}
	
	#if DEBUG
		Serial.println( (error) ? F(" ERROR") : F(" OK"));
	#endif
	
	return error;
}

//#######################################################################
int toggleGPRSPower()
{
	int modemIsOn = 0;
	
	#if DEBUG
		Serial.print( F(" Toggle modem power "));
	#endif
	
	delay(500);
	digitalWrite(9,HIGH);
	delay(2000);
	digitalWrite(9,LOW);
	delay(500);

	const char * const powerStrs[] = {"POWER DOWN\r\n", "RDY\r\n", 0};
	const char * const powerStrs2[] = {"Call Ready\r\n", 0};
	const char * const powerStrs3[] = {"DST: ", 0};
	int match = getResponseUntil( powerStrs);
	
	if( match == -1)
	{
		#if DEBUG
			Serial.print( F(" No response from modem "));
		#endif
		
		modemIsOn = -1;
	}
	else if( match == 0)
	{
		#if DEBUG
			Serial.print( F(" got POWER DOWN "));
		#endif
	}
	else if( match == 1)
	{
		#if DEBUG
			Serial.print( F(" got RDY "));
		#endif
		
		modemIsOn = 1;
		match = getResponseUntil( powerStrs2);
		
		if( match == 0)
		{
			#if DEBUG
				Serial.print( F(" got call ready "));
			#endif
			
			match = getResponseUntil( powerStrs3);
			
			if( match == 0)
			{
				#if DEBUG
					Serial.print( F(" got DST "));
				#endif
				
				getResponse( 10, 500);
			}
		}
	}
	
	return modemIsOn;
}

//#######################################################################
bool setGPRSPower( int onOff, int retry = 1)
{
	int error = 0;
	int toggle = 0;
	
	#if DEBUG
		if( onOff)
		{
			Serial.print( F("Turning on modem "));
		}
		else
		{
			Serial.print( F("Turning off modem "));
		}
	#endif
	
	if( modemPower == -1)
	{
		#if DEBUG
			Serial.println( F(" power state is unknown "));
		#endif
		
		modemPower = toggleGPRSPower();

		#if DEBUG
			Serial.print( F("modem power state is "));
			Serial.println( modemPower);
		#endif
	}
	
	// A 2 Sec. pulse toggles modem on and off
	if( (modemPower == 0) && (onOff == 1)) 
	{
		modemPower = 1;
		toggle = 1;
		
		#if DEBUG
			Serial.print( F(" turning modem on "));
		#endif
	}
	else if( (modemPower == 1) && (onOff == 0)) 
	{
		modemPower = 0;
		toggle = 1;
		
		#if DEBUG
			Serial.print( F(" turning modem off "));
		#endif
	}
	
	if( toggle)
	{
		modemPower = toggleGPRSPower();
	}
	else
	{
		#if DEBUG
			Serial.println( F(" Modem is in requested state "));
		#endif
	}
	
	if( (modemPower != onOff) && (retry == 1))
	{
		// not in requested state -  try again one more time
		setGPRSPower( onOff, 0);
	}

	#if DEBUG
		Serial.println();
	#endif
	
	return (modemPower != onOff);
}

//#######################################################################
int upload()
{
	int error = 0;
	int cmdStep = 1;
	int i = 0;
	
	#if DEBUG
		Serial.println( F("Dialing ..."));
	#endif
	
	error = setGPRSPower( 1);

	if( !error)
	{
		cmdStep++;
		for( i = 0; i < 10; i++)
		{
			if( !error)
			{
				// Verify cell signal
				gprs.println( F("AT+CGATT?")); 
				error = getCommandResponse();
				
				if( !error)
				{
					// AT+CGATT?
					//
					// +CGATT: 1
					//
					// OK
					if( strstr( response, ": 1\r\n") != 0)
					{
						#if DEBUG
							Serial.println(" Signal verified");
						#endif
						break;
					}
					else
					{
						#if DEBUG
							if( i == 0)
							{
								Serial.print("NO Cell Signal");
							}
							else
							{
								Serial.print(".");
							}
						#endif
					}
				}
			}

			delay( 3000);
		}
			
		if( i == 10)
		{
			#if DEBUG
				Serial.println();
			#endif
		}
		
		if( !error && (i == 10))
		{
			error = 1;
		}
	}
	
	if( !error)
	{
		cmdStep++;
		error = syncClock();
	}
	
	if( !error)
	{
		#if DEBUG
			Serial.print( F("CSNS "));
		#endif
		
		// Force phone to operate in data only mode
		cmdStep++;
		gprs.println( F("AT+CSNS=4")); 
		error = getCommandResponse();
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}
	
	if( !error)
	{
		#if DEBUG
			Serial.print( F("CSTT "));
		#endif
		
		cmdStep++;
		// Set APN, user name and password
		// AT+CSTT=\"telargo.t-mobile.com\",\"Emb8b4b\",\"98CB4385\"
		gprs.print( F("AT+CSTT=\"")); 
		gprs.print( apn);
		gprs.print( F("\",\"")); 
		gprs.print( usr);
		gprs.print( F("\",\"")); 
		gprs.print( pwd);
		gprs.print( F("\"")); 
		getResponse( 64, 100);
		
		gprs.println();
		error = getCommandResponse();
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}

	if( !error)
	{
		#if DEBUG
			Serial.print( F("CIICR "));
		#endif
		
		cmdStep++;
		gprs.print( F("AT+CIICR")); 
		getResponse( 64, 100);
		
		gprs.println();
		error = getCommandResponse();
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}
	
	if( !error)
	{
		#if DEBUG
			Serial.print( F("CIFSR "));
		#endif
		
		// Bring up wireless connection
		cmdStep++;
		gprs.println( F("AT+CIFSR")); 
		getResponse( 10, 100);
		// AT+CIFSR
		//
		// 10.122.97.117
		error = getResponse( 17);
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}
	
	if( !error)
	{
		#if DEBUG
			Serial.print( F("CIPSPRT "));
		#endif
		
		// Disable '>' prompt
		cmdStep++;
		gprs.println( F("AT+CIPSPRT=0")); 
		error = getCommandResponse();
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}
	
	if( !error)
	{
		// open a connection to the server
		#if DEBUG
			Serial.print( F("CIPSTART "));
		#endif
		
		cmdStep++;
		gprs.print( F("AT+CIPSTART=\"tcp\",\"h2o.mywebserver.com\",\"80\""));
		getResponse( 64, 100);
		
		// get the first OK/ERROR
		gprs.println();
		error = getCommandResponse();
		
		if( !error)
		{
			// get CONNECT OK/FAIL
			#if DEBUG
				Serial.print( error ? "NO CONNECT " : "CONNECT ");
			#endif
			
			error = getCommandResponse();
		}
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}
	
	if( !error)
	{
		char data[16]; 
		int datalen = 0;
		int reqlen;
		int siteIdLen = strlen( siteId);
		unsigned long t = now();
		int storedPulseCounts = (MAX_COUNT + pulseCountEndIndex - pulseCountStartIndex) % MAX_COUNT + 1;

		
		// max data length = (27 + d + v)*n + 11
		// {"docs":[
		// {"s":DDD,"t":BMMMTTTHHH,"v":BMMMTTTHHH},
		// ]}
		for( int i = 0; i < storedPulseCounts; i++)
		{
			datalen += siteIdLen;
			
			ultoa( t, data, 10);
			datalen += strlen( data);
			
			ultoa( pulseCount[(pulseCountStartIndex + i) % MAX_COUNT], data, 10);
			datalen += strlen( data);
			
			datalen += 17;
		}
		
		datalen += (11 - 1);  // -1 because last line won't have a comma
		
		reqlen = 185 + datalen;
		
		if( datalen >   99) reqlen++;
		if( datalen >  999) reqlen++;
		if( datalen > 9999) reqlen++;
		
		#if DEBUG >= 3
			Serial.print( "datalen = ");
			Serial.println( datalen);
			Serial.print( "reqlen = ");
			Serial.println( reqlen);
		#endif
		
		#if DEBUG
			Serial.print( F("CIPSEND "));
		#endif
		
		gprs.print( F("AT+CIPSEND="));
		gprs.println( reqlen);
		getResponse( 50, 500);
		
		gprs.print( F( "POST http://h2o.mywebserver.com/_bulk_docs HTTP/1.1\r\n")); 	//  57
		getResponse( 50, 100);
		gprs.print( F( "Host: h2o.mywebserver.com\r\n"));					//  31
		getResponse( 50, 100);
		gprs.print( F( "Content-Type: application/json\r\n"));					//  32
		getResponse( 50, 100);
		gprs.print( F( "Authorization: Basic asdf780987saf78dd\r\n"));		//  43
		getResponse( 50, 100);
		gprs.print( F( "Content-Length: "));									//  16
		gprs.print( datalen);													//   2 or 3 or 4
		gprs.print( F( "\r\n\r\n"));											//   4
		getResponse( 50, 100);
		
		data[0] = 0;
		gprs.print( F( "{\"docs\":["));

		for( int i = 0; i < storedPulseCounts; i++)
		{
			// {"site":DDD,"index":BMMMTTTHHH,"value\":BMMMTTTHHH} 
			gprs.print( F( "{\"s\":")); 
			gprs.print( siteId);
			
			gprs.print( F( ",\"t\":")); 
			ultoa( t - ((storedPulseCounts - i) * interval_s), data, 10);
			gprs.print( data);
			
			gprs.print( F( ",\"v\":")); 
			ultoa( pulseCount[(pulseCountStartIndex + i) % MAX_COUNT], data, 10);
			gprs.print( data);
			
			gprs.print( F( "}"));
			
			if( i < pulseCountEndIndex)
			{
				gprs.print( F( ","));
			}
			
			getResponse( 100, 100);
		}
		
		gprs.print( F( "]}"));
		getResponse( 2, 100);
		
		cmdStep++;
		error = getCommandResponse();
		
		#if DEBUG
			Serial.print( error ? "SEND FAIL " : "SEND OK ");
		#endif
		
		if( !error)
		{
			getResponse( 255, 5000);  // the response is ~368 chars - too much to handle at once
									  // the 1st 128 should come in ok
			
			// check for 201 ???
		}
	}
	
	if( !error)
	{
		// close connection to the server
		#if DEBUG
			Serial.print( F("CIPCLOSE "));
		#endif
		
		cmdStep++;
		gprs.println( F("AT+CIPCLOSE")); 
		getCommandResponse();
		getResponse( 100, 5000);
		
		#if DEBUG
			Serial.print( error ? "ERROR " : "OK ");
		#endif
	}
	
	#if DEBUG
		Serial.println();
	#endif
	
	setGPRSPower( 0);
	
	if( !error)
	{
		cmdStep = 0;
	}
	
	return cmdStep;
}

//#######################################################################
void setup()
{
	gprs.begin(19200);
	#if DEBUG
		Serial.begin( 19200);
	#endif
	delay(500);
	
	// Pin 9 controls modem power
	pinMode( 9, OUTPUT); 
	digitalWrite( 9,LOW);

	// use the LED on P13 to indicate water flow
	pinMode( 13, OUTPUT); 
	digitalWrite( 13,HIGH);
		
	pulseCountStartIndex = 0;
	pulseCountEndIndex = 0;
	
	for( int i = 0; i < MAX_COUNT; i++)
	{
		pulseCount[i] = 0;
	}

	nextBin = millis() + interval;
	nextFlowDetect = millis() + flowInterval;
	
	#if DEBUG
		Serial.println();
		Serial.println( F("Starting"));
		Serial.print( F("sample period: ")); Serial.println( interval);
		Serial.print( F("flow detect period: ")); Serial.println( flowInterval);
		Serial.print( F("Free memory: ")); Serial.println( memoryFree());

		Serial.println( F("press a key to start"));
		while(1)
		{
			if (Serial.available()) 
			{
				Serial.read();
				break;
			}
		}
		
		//Serial.println( upload());
	#endif

	// enable interrupt 0 using pin 2
	attachInterrupt( 0, pulseISR, FALLING);
}

//#######################################################################
void loop()
{
	unsigned long now = millis();
	unsigned long count;
	
	if( now >= nextBin)
	{
		int retry;
		int errorCode = 0;
		
		noInterrupts();
		pulseCount[pulseCountEndIndex] = currentPulseCount;
		currentPulseCount = 0;
		interrupts();

		#if DEBUG
			Serial.print( F("index "));
			Serial.print( pulseCountEndIndex);
			Serial.print( F(": "));
			Serial.println( pulseCount[pulseCountEndIndex]);
		#endif
		
		int storedPulseCounts = (MAX_COUNT + pulseCountEndIndex - pulseCountStartIndex) % MAX_COUNT + 1;
		
		nextBin = millis() + interval;
		
		if( storedPulseCounts >= SEND_COUNT)
		{
			for( retry = 0; retry < 3; retry++)
			{
				// retry 3 times
				errorCode = upload();
				
				if( errorCode == 0)
				{
					break;
				}
				
				#if DEBUG
					Serial.print( F("errorCode: "));
					Serial.println( errorCode);
				#endif
				
				delay( 5000);
			}			
		
			if( errorCode)
			{
				// upload failed
				failCount++;
		
				pulseCountEndIndex = (pulseCountEndIndex + 1) % MAX_COUNT;
				
				if( pulseCountEndIndex < pulseCountStartIndex)
				{
					pulseCountStartIndex = pulseCountEndIndex;
				}
			}
			else
			{
				pulseCountStartIndex = 0;
				pulseCountEndIndex = 0;
					
				#if DEBUG
					Serial.println( F("array reset"));
				#endif
			}
		}
		else
		{
			pulseCountEndIndex = (pulseCountEndIndex + 1) % MAX_COUNT;
			
			if( pulseCountEndIndex < pulseCountStartIndex)
			{
				pulseCountStartIndex = pulseCountEndIndex;
			}
		}
	}
	
	if( now >= nextFlowDetect)
	{
		nextFlowDetect = now + flowInterval;
		
		noInterrupts();
		count = currentPulseCount;
		interrupts();
		
		#if DEBUG >= 2
			Serial.print( count);
			Serial.print( "  ");
			Serial.println( (nextBin - now)/1000);
		#endif
		
		if( prevFlowCount > 0)
		{
			if( (count - prevFlowCount) > 0)
			{
				digitalWrite( 13, LOW);
			}
			else
			{
				digitalWrite( 13, HIGH);
			}
		}
		
		prevFlowCount = count;
	}
}
