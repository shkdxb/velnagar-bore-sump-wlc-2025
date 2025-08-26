// Example: Serial command handler for updating variables
void initializeSerialCommands();
void handleSerialCommands();
void parseCommand(String cmd);

void initializeSerialCommands()
{
  Serial.println("Ready for commands:");
  Serial.println("Examples (in small leters):");
  Serial.println(" ncx=120   -> set needle DIAL_CENTRE_X = 120");
  Serial.println(" ncy=75   -> set needle DIAL_CENTRE_Y = 75");
  Serial.println(" ndlr=25   -> set NEEDLE_RADIUS = 25");
  Serial.println(" ndll=50   -> set NEEDLE_LENGTH = 50");
  Serial.println(" dpx=0   -> set byte DIAL_POSITION_X =0");
  Serial.println(" dpy=0   -> set byte DIAL_POSITION_X =0");
}

void handleSerialCommands()
{
  static String inputString = "";

  while (Serial.available())
  {
    char inChar = (char)Serial.read();

    if (inChar == '\n')
    { // Command finished
      inputString.trim();
      parseCommand(inputString);
      inputString = "";
    }
    else
    {
      inputString += inChar;
    }
  }
}

void parseCommand(String cmd)
{
  Serial.print("Received: ");
  Serial.println(cmd);
  if (cmd.startsWith("ncx="))
  {
    DIAL_CENTRE_X = cmd.substring(4).toInt();
    Serial.printf("needle DIAL_CENTRE_X set to %d \n", DIAL_CENTRE_X);
  }
  else if (cmd.startsWith("ncy="))
  {
    DIAL_CENTRE_Y = cmd.substring(4).toInt();
    Serial.printf("needle DIAL_CENTRE_Y set to %d \n", DIAL_CENTRE_Y);
  }
  else if (cmd.startsWith("ndlr="))
  {
    NEEDLE_RADIUS = cmd.substring(5).toInt();
    Serial.printf("NEEDLE_RADIUS set to %d \n", NEEDLE_RADIUS);
  }
  else if (cmd.startsWith("ndll="))
  {
    NEEDLE_LENGTH = cmd.substring(5).toInt();
    Serial.printf("NEEDLE_LENGTH set to %d \n", NEEDLE_LENGTH);
  }
  else if (cmd.startsWith("dpx="))
  {
    DIAL_POSITION_X = cmd.substring(4).toInt();
    Serial.printf("DIAL_POSITION_X set to %d \n", DIAL_POSITION_X);
  }
  else if (cmd.startsWith("dpy="))
  {
    DIAL_POSITION_Y = cmd.substring(4).toInt();
    Serial.printf("DIAL_POSITION_Y set to %d \n", DIAL_POSITION_Y);
  }
  else
  {
    Serial.println("Unknown command!");
  }
}
