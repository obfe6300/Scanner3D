/* Arduino Pro Micro Scanner Code (DIY 3D Scanner - Super Make Something Episode 8) - https://youtu.be/-qeD2__yK4c
   by: Alex - Super Make Something
   date: January 2nd, 2016
   license: Creative Commons - Attribution - Non-Commercial.  More information available at: http://creativecommons.org/licenses/by-nc/3.0/
*/

// Includes derivative of "ReadWrite" by David A. Mellis and Tom Igoe available at: https://www.arduino.cc/en/Tutorial/ReadWrite
// Includes derivative of EasyDriver board sample code by Joel Bartlett available at: https://www.sparkfun.com/tutorials/400

/*
   This code contains the follow functions:
   - void setup(): initializes Serial port, SD card
   - void loop(): main loop
   - double readAnalogSensor(): calculates sensed distances in cm.  Sensed values calculated as an average of noSamples consecutive analogRead() calls to eliminate noise
   - void writeToSD(): Writes sensed distance in cm to SD card file specified by filename variable
   - void readFromSD(): Prints out contents of SD card file specified by filename variable to Serial
*/

/*
  Pinout:
  SD card attached to SPI bus as follows:
  - MOSI - pin 16
  - MISO - pin 14
  - CLK - pin 15
  - CS - pin 10

  IR Sensor (SHARP GP2Y0A51SK0F: 2-15cm, 5V) attached to microcontroller as follows:
  - Sense - A3

  Turntable stepper motor driver board:
  - STEP - pin 2
  - DIR - pin 3
  - MS1 - pin 4
  - MS2 - pin 5
  - Enable - pin 6

  Z-Axis stepper motor driver board:
  - STEP - pin 7
  - DIR - pin 8
  - MS1 - pin 9
  - MS2 - pin 18 (A0 on Arduino Pro Micro silkscreen)
  - ENABLE - pin 19 (A1 on Arduino Pro Micro silkscreen)
*/

#include <SPI.h>
#include <SD.h>

#define NB_SAMPLES  100

File scannerValues;
String filename = "scn000.txt";
int csPin = 10;
int sensePin = A3;

int tStep = 2;
int tDir = 3;
int tMS1 = 4;
int tMS2 = 5;
int tEnable = 6;

int zStep = 7;
int zDir = 8;
int zMS1 = 9;
int zMS2 = 18;
int zEnable = 19;

/*******************************************
 * 
 *          S E T U P
 * 
 *******************************************/
void setup()
{

  //Define stepper pins as digital output pins
  pinMode(tStep, OUTPUT);
  pinMode(tDir, OUTPUT);
  pinMode(tMS1, OUTPUT);
  pinMode(tMS2, OUTPUT);
  pinMode(tEnable, OUTPUT);
  pinMode(zStep, OUTPUT);
  pinMode(zDir, OUTPUT);
  pinMode(zMS1, OUTPUT);
  pinMode(zMS2, OUTPUT);
  pinMode(zEnable, OUTPUT);

  //Set microstepping mode for stepper driver boards.  Using 1.8 deg motor angle (200 steps/rev) NEMA 17 motors (12V)

  /*
    // Theta motor: 1/2 step micro stepping (MS1 High, MS2 Low) = 0.9 deg/step (400 steps/rev)
    digitalWrite(tMS1,HIGH);
    digitalWrite(tMS2,LOW);
  */


  // Theta motor: no micro stepping (MS1 Low, MS2 Low) = 1.8 deg/step (200 steps/rev)
  digitalWrite(tMS1, LOW);
  digitalWrite(tMS2, LOW);


  // Z motor: no micro stepping (MS1 Low, MS2 Low) = 1.8 deg/step (200 steps/rev) --> (200 steps/1cm, i.e. 200 steps/10 mm).  Therefore 0.05mm linear motion/step.
  digitalWrite(zMS1, LOW);
  digitalWrite(zMS2, LOW);

  //Set rotation direction of motors
  //digitalWrite(tDir,HIGH);
  //digitalWrite(zDir,LOW);
  //delay(100);

  //Enable motor controllers
  digitalWrite(tEnable, LOW);
  digitalWrite(zEnable, LOW);

  // Open serial communications
  Serial.begin(9600);
  while (!Serial) {
    delay(20);
  }
  Serial.println("\n");

  // calibrage hauteur du plateau
  calibrageZ();

  // boucle d'arret ajoutee temporairement pour valider la calibration
  while (1);

  //Debug to Serial
  Serial.print("Initializing SD card... ");
  if (!SD.begin(csPin))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization success!");
}

/*******************************************
 * 
 *          L O O P
 * 
 *******************************************/
void loop()
{

  int vertDistance = 10; //Total desired z-axis travel
  int noZSteps = 20; //No of z-steps per rotation.  Distance = noSteps*0.05mm/step
  int zCounts = (200 / 1 * vertDistance) / noZSteps; //Total number of zCounts until z-axis returns home
  //int thetaCounts=400;
  int thetaCounts = 200;

  // Scan object
  digitalWrite(zDir, LOW);
  for (int j = 0; j < zCounts; j++) //Rotate z axis loop
  {
    for (int i = 0; i < thetaCounts; i++) //Rotate theta motor for one revolution, read sensor and store
    {
      rotateMotor(tStep, 1); //Rotate theta motor one step
      delay(200);
      double senseDistance = 0; //Reset senseDistanceVariable;
      senseDistance = readAnalogSensor(); //Read IR sensor, calculate distance
      writeToSD(senseDistance); //Write distance to SD
    }

    rotateMotor(zStep, noZSteps); //Move z carriage up one step
    delay(1000);
    writeToSD(9999); //Write dummy value to SD for later parsing
  }

  // Scan complete.  Rotate z-axis back to home and pause.
  digitalWrite(zDir, HIGH);
  delay(10);
  for (int j = 0; j < zCounts; j++)
  {
    rotateMotor(zStep, noZSteps);
    delay(10);
  }

  for (int k = 0; k < 3600; k++) //Pause for one hour (3600 seconds), i.e. freeze until power off because scan is complete.
  {
    delay(1000);
  }

  //readFromSD(); //Debug - Read from SD
}

/*******************************************
 * 
 *          calibrageZ
 * 
 *******************************************/
void calibrageZ() {
  int distancePlateau = 400;
  int distanceSupport = 800;
  int tolerance = 100;
  int minPlateau = distancePlateau - tolerance;
  int maxPlateau = distancePlateau + tolerance;
  int minSupport = distanceSupport - tolerance;
  int maxSupport = distanceSupport + tolerance;
  int senseValue;
  char texte[50];
  int delai = 100;

  sprintf(texte, "Start calibration of Z axis");
  Serial.println(texte);

  senseValue = readAnalogSensor(); //Perform analogRead
  sprintf(texte, "valeur %d (%d) : min=%d, max=%d", senseValue, distancePlateau, minPlateau, maxPlateau);
  Serial.println(texte);
  // phase 1 on on descend jusqu'au plateau
  boolean enFaceDuPlateau = (senseValue < minPlateau && senseValue > maxPlateau);
  while (!enFaceDuPlateau) {
    // on n'est pas en face du support
    // on fait descendre le plateau de 10 pas
    rotateMotor(zStep, -10);
    sprintf(texte, "on descends de 10 pas");
    Serial.println(texte);
    delay(delai);
  }
  // phase 2 on est en face du plateau on monte
  while (enFaceDuPlateau) {
    // on est en face du plateau
    // on fait monter le plateau d'un pas
    rotateMotor(zStep, 1);
    sprintf(texte, "on monte d'un pas");
    Serial.println(texte);
    delay(delai);
  }
  // phase 3 on est au dessus du plateau
  // on redessend d'un pas
  rotateMotor(zStep, -1);
  sprintf(texte, "on descend d'un pas");
  Serial.println(texte);
  delay(delai);
  sprintf(texte, "calibration axe Z OK");
  Serial.println(texte);
}

/*******************************************
 * 
 *          rotateMotor
 * 
 *******************************************/
void rotateMotor(int pinNo, int steps)
{

  for (int i = 0; i < steps; i++)
  {
    digitalWrite(pinNo, LOW); //LOW to HIGH changes creates the
    delay(1);
    digitalWrite(pinNo, HIGH); //"Rising Edge" so that the EasyDriver knows when to step.
    delay(1);
    //delayMicroseconds(500); // Delay so that motor has time to move
    //delay(100); // Delay so that motor has time to move
  }
}

/*******************************************
 * 
 *          readAnalogSensor
 * 
 *******************************************/
double readAnalogSensor()
{

  int noSamples = NB_SAMPLES;
  int sumOfSamples = 0;
  int tblSamples[NB_SAMPLES / 2];
  int tblSamplesSize = noSamples / 2;
  int ptrTblSample = 0;
  boolean tblSampleFull = false;
  int moyenne, minValue, maxValue, oldValue;
  int ptrMaxValue = -1, ptrMinValue = -1, ptrminPlateau = -1;
  char texte[50];

  int senseValue = 0;
  double senseDistance = 0;

  sprintf(texte, "nb samples =  %d", noSamples);
  Serial.println(texte);
  sprintf(texte, "tab size =  %d", tblSamplesSize);
  Serial.println(texte);

  for (int i = 0; i < noSamples; i++)
  {
    senseValue = analogRead(sensePin); //Perform analogRead
    delay(2); //Delay to let analogPin settle -- not sure if necessary

    // bloc ajoute par BFR pour filtrer les valeurs extremes
    // a verifier (surtout les depassements de tableau)
    if (!tblSampleFull) {
      // le tableau n'est pas plein, on stocke toutes les valeurs qui arrivent
      sprintf(texte, "valeur %d : init tab[%d] =  %d", i, ptrTblSample, senseValue);
      Serial.println(texte);
      tblSamples[ptrTblSample++] = senseValue;
      sumOfSamples = sumOfSamples + senseValue; //Running sum of sensed distances
      if (ptrTblSample >= tblSamplesSize) {
        tblSampleFull = true;
        sprintf(texte, "tblSample Full");
        Serial.println(texte);
      }
    } else {
      moyenne = sumOfSamples / tblSamplesSize;
      if ((senseValue > moyenne * 0, 25) || (senseValue < moyenne * 0, 75)) {
        sprintf(texte, "valeur %d : acceptable : %d, ", i, senseValue);
        Serial.print(texte);
        // la valeur mesuree est dans la plage acceptable, on la garde
        // on calcule les valeurs max et min du tableau
        minValue = 9999;
        maxValue = 0;
        for (int j = 0 ; j < tblSamplesSize; j++) {
          if (tblSamples[j] > maxValue) {
            // on memorise la valeur max
            maxValue = tblSamples[j];
            ptrMaxValue = j;
          }
          if (tblSamples[j] < minValue) {
            // on memorise la valeur min
            minValue = tblSamples[j];
            ptrMinValue = j;
          }
        }
        // on remplace la valeur mesuree acceptable par la
        // valeur la plus grande ou plus petite du tableau
        // et on met a jour la somme
        if (senseValue > moyenne && senseValue < tblSamples[ptrMaxValue]) {
          // on retranche l'ancienne valeur de la somme
          oldValue = tblSamples[ptrMaxValue];
          sumOfSamples -= oldValue;
          // on remplace la valeur la plus grande par celle mesuree
          tblSamples[ptrMaxValue] = senseValue;
          sprintf(texte, "remplace %d par %d a la position %d, ", oldValue, senseValue, ptrMaxValue);
          Serial.println(texte);
          // on ajoute la nouvelle valeur a la somme
          sumOfSamples += tblSamples[ptrMaxValue];
        } else if (senseValue <= moyenne && senseValue > tblSamples[ptrMinValue]) {
          // on retranche l'ancienne valeur de la somme
          oldValue = tblSamples[ptrMinValue];
          sumOfSamples -= oldValue;
          // on remplace la valeur la plus petite par celle mesuree
          tblSamples[ptrMinValue] = senseValue;
          sprintf(texte, "remplace %d par %d a la position %d, ", oldValue, senseValue, ptrMinValue);
          Serial.println(texte);
          // on ajoute la nouvelle valeur a la somme
          sumOfSamples += tblSamples[ptrMinValue];
        } else {
          
        }
      } else {
        // la valeur mesuree n'est pas dans la plage acceptable, on ne fait rien
        sprintf(texte, "valeur %d : pas entre 25% et 75% : %d, ", i, senseValue);
        Serial.print(texte);
      }
    }
    // fin du bloc ajoute par BFR
  } // fin de la boucle sur les 100 mesures
  senseValue = sumOfSamples / tblSamplesSize; //Calculate mean
  sprintf(texte, "moyenne = %d", senseValue);
  Serial.println(texte);
  senseDistance = senseValue; //Convert to double
  senseDistance = mapDouble(senseDistance, 0.0, 1023.0, 0.0, 5.0); //Convert analog pin reading to voltage
  Serial.print("Voltage: ");     //Debug
  Serial.println(senseDistance);   //Debug
  //Serial.print(" | "); //Debug
  senseDistance = -5.40274 * pow(senseDistance, 3) + 28.4823 * pow(senseDistance, 2) - 49.7115 * senseDistance + 31.3444; //Convert voltage to distance in cm via cubic fit of Sharp sensor datasheet calibration
  //Print to Serial - Debug
  //Serial.print("Distance: ");    //Debug
  //Serial.println(senseDistance); //Debug
  //Serial.print(senseValue);
  //Serial.print("   |   ");
  //Serial.println(senseDistance);
  return senseDistance;
}

/*******************************************
 * 
 *          writeToSD
 * 
 *******************************************/
void writeToSD(double senseDistance)
{
  // Open file
  scannerValues = SD.open(filename, FILE_WRITE);

  // If the file opened okay, write to it:
  if (scannerValues)
  {
    //Debug to Serial
    /*
      Serial.print("Writing to ");
      Serial.print(filename);
      Serial.println("...");
    */

    //Write to file
    scannerValues.print(senseDistance);
    scannerValues.println();

    // close the file:
    scannerValues.close();

    //Debug to Serial
    //Serial.println("done.");
  }
  else
  {
    // if the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(filename);
  }
}

/*******************************************
 * 
 *          readFromSD
 * 
 *******************************************/
void readFromSD()
{
  // Open the file for reading:
  scannerValues = SD.open(filename);
  if (scannerValues)
  {
    Serial.print("Contents of ");
    Serial.print(filename);
    Serial.println(":");

    // Read from file until there's nothing else in it:
    while (scannerValues.available())
    {
      Serial.write(scannerValues.read());
    }
    // Close the file:
    scannerValues.close();
  }
  else
  {
    // If the file didn't open, print an error:
    Serial.print("error opening ");
    Serial.println(filename);
  }
}

/*******************************************
 * 
 *          mapDouble
 * 
 *******************************************/
double mapDouble(double x, double in_min, double in_max, double out_min, double out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

