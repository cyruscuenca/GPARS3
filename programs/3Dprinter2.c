#pragma config(Motor,  motorA,          extruderButton, tmotorEV3_Medium, PIDControl, encoder)
#pragma config(Motor,  motorB,          z_axis,        tmotorEV3_Large, PIDControl, encoder)
#pragma config(Motor,  motorC,          x_axis,        tmotorNXT, PIDControl, encoder)
#pragma config(Motor,  motorD,          y_axis,        tmotorNXT, PIDControl, encoder)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//

//Opens debug stream
#pragma DebuggerWindows("debugStream");

// Comment out the line below if you want the motors to run
//#define DISABLE_MOTORS

// name of the file it'll be reading
//:filename
const char *fileName = "gcode.txt";


// You need some kind of value here that will never be used in your g-code
const float noParam = -255;

const long EOF = -255;

typedef enum tCmdType
{
	GCMD_NONE,
	GCMD_G1,
	GCMD_G92,
	GCMD_X,
	GCMD_Y,
	GCMD_Z,
	GCMD_E,
	GCMD_F,
} tCmdType;

#ifndef DISABLE_MOTORS
void waitForMotors();
#endif

float calcDeltaDistance(float &currentPosition, float newPosition);
float calcMotorDegrees(float deltaPosition, long degToMM);
void moveMotorAxis(tMotor axis, float degrees, long motorPower);
void startSeq();
void endSeq();
tCmdType processesCommand(char *buff, int buffLen, float &cmdVal);
bool readNextCommand(char *cmd, int cmdLen, tCmdType &gcmd, float &x, float &y, float &z, float &e, float &f);
void executeCommand(tCmdType gcmd, float x, float y, float z, float e, float f);
long readLine(long fd, char *buffer, long buffLen);
void handleCommand_G1(float x, float y, float z, float e, float f);
void handleCommand_G92(float x, float y, float z, float e, float f);
long degBuff;

//this is where you specify your starting position
//:startposition
float xAxisPosition, yAxisPosition, zAxisPosition = 0;

//this is where you specify the degrees to mm so the program can compensate properly
//:degreestomm
long xDegreesToMM = 8;
long yDegreesToMM = 8;
long zDegreesToMM = 8;

//this is where you can set the median speed of the motors
//:setspeed
long setPower = 9;

long xPower, yPower = setPower;

task main(){
	clearDebugStream();
	setLEDColor(ledRed);

	long isStartPressed = 0;
	long slideNum = 1; //slideNum holds what number slide is being displayed
	drawBmpfile(0, 127, "slide1");
	sleep(2000);
	while(isStartPressed == 0){
		waitForButtonPress();
		long isLeftPressed = getButtonPress(LEFT_BUTTON);
		long isRightPressed = getButtonPress(RIGHT_BUTTON);
		long isEnterPressed = getButtonPress(ENTER_BUTTON);

		if(isLeftPressed == 1){
			playTone(554, 5);
			slideNum = slideNum - 1;
		}
		if(isRightPressed == 1){
			playTone(554, 5);
			slideNum = slideNum + 1;
		}
		if(slideNum == 1 && isEnterPressed == 1){
			playTone(554, 5);
			isStartPressed = 1;
			sleep(200);
		}
		if(slideNum == 0){
			slideNum = 3;
		}
		if(slideNum == 4){
			slideNum = 1;
		}
		if(slideNum == 1){
			drawBmpfile(0, 127, "slide1");
		}
		if(slideNum == 2){
			drawBmpfile(0, 127, "slide2");
		}
		if(slideNum == 3){
			drawBmpfile(0, 127, "slide3");
		}
		sleep(250);
	}

	startSeq();
	setLEDColor(ledOff);

	float x, y, z, e, f = 0.0;
	long fd = 0;
	char buffer[128];
	long lineLength = 0;

	tCmdType gcmd = GCMD_NONE;

	fd = fileOpenRead(fileName);

	if (fd < 0) // if file is not found/cannot open
	{
		writeDebugStreamLine("Could not open %s", fileName);
		return;
	}

	while (true)
	{
		lineLength = readLine(fd, buffer, 128);
		if (lineLength != EOF)
		{
			// We can ignore empty lines
			if (lineLength > 0)
			{
				// The readNextCommand will only return true if a valid command has been found
				// Comment handling is now done there.
				if (readNextCommand(buffer, lineLength, gcmd, x, y, z, e, f))
					executeCommand(gcmd, x, y, z, e, f);
				// Wipe the buffer by setting its contents to 0
			}
			memset(buffer, 0, sizeof(buffer));
		}
		else
		{
			// we're done
			writeDebugStreamLine("All done!");
			endSeq();
			return;
		}
	}
}

#ifndef DISABLE_MOTORS
void waitForMotors(){
	while(getMotorRunning(x_axis) || getMotorRunning(y_axis) || getMotorRunning(z_axis)){
		sleep(1);
	}
}
#endif

// Calculate the distance (delta) from the current position to the new one
// and update the current position
float calcDeltaDistance(float &currentPosition, float newPosition){

	writeDebugStreamLine("calcDeltaDistance(Current position: %f, New position: %f)", currentPosition, newPosition);

	float deltaPosition = newPosition - currentPosition;
	writeDebugStreamLine("deltaPosition: %f", deltaPosition);
	currentPosition = newPosition;
	writeDebugStreamLine("Updated currentPosition: %f", currentPosition);
	return deltaPosition;
}

// Calculate the degrees the motor has to turn, using provided gear size
float calcMotorDegrees(float deltaPosition, long degToMM)
{
	writeDebugStreamLine("calcMotorDegrees(%f, %f)", deltaPosition, degToMM);
	return deltaPosition * degToMM;
}

// Wrapper to move the motor, provides additional debugging feedback
void moveMotorAxis(tMotor axis, float degrees, long motorPower)
{
	writeDebugStreamLine("moveMotorAxis: motor: %d, rawDegrees: %f, power: %d", axis, degrees, motorPower);
#ifndef DISABLE_MOTORS
	long roundedDegrees = round(degrees);
	degBuff = degrees - roundedDegrees + degBuff;
	if (roundedDegrees < 0)
	{
		roundedDegrees = abs(roundedDegrees);
		motorPower *= -1;
	}
	if (degBuff > 1 || degBuff < -1){
		int degBuffRounded = round(degBuff);
		roundedDegrees = roundedDegrees + degBuffRounded;
		degBuff = degBuff - degBuffRounded;
	}
	moveMotorTarget(axis, roundedDegrees, motorPower);
#endif
	return;
}

// We're passed a single command, like "G1" or "X12.456"
// We need to split it up and pick the value type (X, or Y, etc) and float value out of it.
tCmdType processesCommand(char *buff, int buffLen, float &cmdVal)
{
	cmdVal = noParam;
	int gcmdType = -1;

	// Anything less than 2 characters is bogus
	if (buffLen < 2)
		return GCMD_NONE;

	switch (buff[0])
	{
	case 'G':
		sscanf(buff, "G%d", &gcmdType);
		switch(gcmdType)
		{
		case 1: return GCMD_G1;
		case 92: return GCMD_G92;
		default: return GCMD_NONE;
		}
	case 'X':
		sscanf(buff, "X%f", &cmdVal); return GCMD_X;

	case 'Y':
		sscanf(buff, "Y%f", &cmdVal); return GCMD_Y;

	case 'Z':
		sscanf(buff, "Z%f", &cmdVal); return GCMD_Z;

	case 'E':
		sscanf(buff, "E%f", &cmdVal); return GCMD_E;

	case 'F':
		sscanf(buff, "F%f", &cmdVal); return GCMD_F;

	default: return GCMD_NONE;
	}
}


// Read and parse the next line from file and retrieve the various parameters, if present
bool readNextCommand(char *cmd, int cmdLen, tCmdType &gcmd, float &x, float &y, float &z, float &e, float &f)
{
	char currCmdBuff[16];
	tCmdType currCmd = GCMD_NONE;
	int currCmdBuffIndex = 0;
	float currCmdVal = 0;

	x = y = z = e = f = noParam;
	writeDebugStreamLine("\n----------    NEXT COMMAND   -------");
	writeDebugStreamLine("Processing: %s", cmd);

	// Clear the currCmdBuff
	memset(currCmdBuff, 0, sizeof(currCmdBuff));

	// Ignore if we're starting with a ";"
	if (cmd[0] == ';')
		return false;

	for (int i = 0; i < cmdLen; i++)
	{
		currCmdBuff[currCmdBuffIndex] = cmd[i];
		// We process a command whenever we see a space or the end of the string, which is always a 0 (NULL)
		if ((currCmdBuff[currCmdBuffIndex] == ' ') || (currCmdBuff[currCmdBuffIndex] == 0))
		{
			currCmd = processesCommand(currCmdBuff, currCmdBuffIndex + 1, currCmdVal);
			// writeDebugStreamLine("currCmd: %d, currCmdVal: %f", currCmd, currCmdVal);
			switch (currCmd)
			{
			case GCMD_NONE: gcmd = GCMD_NONE; return false;
			case GCMD_G1: gcmd = GCMD_G1; break;
			case GCMD_G92: gcmd = GCMD_G92; break;
			case GCMD_X: x = currCmdVal; break;
			case GCMD_Y: y = currCmdVal; break;
			case GCMD_Z: z = currCmdVal; break;
			case GCMD_E: e = currCmdVal; break;
			case GCMD_F: f = currCmdVal; break;
			}
			// Clear the currCmdBuff
			memset(currCmdBuff, 0, sizeof(currCmdBuff));

			// Reset the index
			currCmdBuffIndex = 0;
		}
		else
		{
			// Move to the next buffer entry
			currCmdBuffIndex++;
		}
	}

	return true;
}

// Use parameters gathered from command to move the motors, extrude, that sort of thing
void executeCommand(tCmdType gcmd, float x, float y, float z, float e, float f)
{
	// execute functions inside this algorithm
	switch (gcmd)
	{
	case GCMD_G1:
		handleCommand_G1(x, y, z, e, f);
		break;
	case GCMD_G92:
		handleCommand_G92(x, y, z, e, f);
		break;
	default:
		displayCenteredBigTextLine(1 , "error! Gcmd value is unknown!");
		break;
	}

}

// Read the file, one line at a time
long readLine(long fd, char *buffer, long buffLen)
{
	long index = 0;
	char c;

	// Read the file one character at a time until there's nothing left
	// or we're at the end of the buffer
	while (fileReadData(fd, &c, 1) && (index < (buffLen - 1)))
	{
		//writeDebugStreamLine("c: %c (0x%02X)", c, c);
		switch (c)
		{
			// If the line ends in a newline character, that tells us we're at the end of that line
			// terminate the string with a \0 (a NULL)
		case '\r': break;
		case '\n': buffer[index] = 0; return index;
			// It's something other than a newline, so add it to the buffer and let's continue
		default: buffer[index] = c; index++;
		}
	}
	// Make sure the buffer is NULL terminated
	buffer[index] = 0;
	if (index == 0)
		return EOF;
	else
		return index;  // number of characters in the line
}

void handleCommand_G1(float x, float y, float z, float e, float f)
{
	writeDebugStreamLine("Handling G1 command");
	float motorDegrees; 	// Amount the motor has to move
	float deltaPosition;	// The difference between the current position and the one we want to move to

	if((x != noParam) && (y != noParam)){

		//the power calculation is so both motors stop moving
		//at the same time
		deltaPosition = calcDeltaDistance(xAxisPosition, x);
		float xDegrees = calcMotorDegrees(deltaPosition, xDegreesToMM);
		deltaPosition = calcDeltaDistance(yAxisPosition, y);
		float yDegrees = calcMotorDegrees(deltaPosition, yDegreesToMM);
		//math
		//y-power = y-dist/x-dist *,x-power
		moveMotorAxis(x_axis, xDegrees, xPower);
		moveMotorAxis(y_axis, yDegrees, yPower);
	}

	if((x != noParam) && (y == noParam)){
		writeDebugStreamLine("\n----------    X AXIS   -------------");
		deltaPosition = calcDeltaDistance(xAxisPosition, x);
		motorDegrees = calcMotorDegrees(deltaPosition, xDegreesToMM);
		moveMotorAxis(x_axis, motorDegrees, setPower);
	}

	if((y != noParam) && (x == noParam)){
		writeDebugStreamLine("\n----------    Y AXIS   -------------");
		deltaPosition = calcDeltaDistance(yAxisPosition, y);
		motorDegrees = calcMotorDegrees(deltaPosition, yDegreesToMM);
		moveMotorAxis(y_axis, motorDegrees, setPower);
	}

	if(z != noParam){
		writeDebugStreamLine("\n----------    Z AXIS   -------------");
		deltaPosition = calcDeltaDistance(zAxisPosition, z);
		motorDegrees = calcMotorDegrees(deltaPosition, zDegreesToMM);
		moveMotorAxis(z_axis, motorDegrees, setPower);
	}

#ifndef DISABLE_MOTORS
	waitForMotors();
#endif
}

void handleCommand_G92(float x, float y, float z, float e, float f){
	writeDebugStreamLine("Handling G92 command");
	if(x != noParam){
		writeDebugStreamLine("\n----------    X AXIS   -------------");
		xAxisPosition = x;
	}

	if(y != noParam){
		writeDebugStreamLine("\n----------    Y AXIS   -------------");
		yAxisPosition = y;
	}

	if(z != noParam){
		writeDebugStreamLine("\n----------    Z AXIS   -------------");
		zAxisPosition = z;
	}
}

void startSeq(){
	setLEDColor(ledRed);

	playTone(554, 5);
	sleep(1000);
	playTone(554, 5);
	sleep(1000);
	playTone(554, 5);
	sleep(1000);
#ifndef DISABLE_MOTORS
	moveMotorTarget(extruderButton, 75, -100);
#endif
}

void endSeq(){
	setLEDColor(ledGreen);

#ifndef DISABLE_MOTORS
	moveMotorTarget(extruderButton, 75, 100);
#endif
	playTone(554, 5);
	sleep(10000);
	playTone(554, 5);
	sleep(10000);
	playTone(554, 5);
	sleep(10000);
}
