#include <SoftwareSerial.h>
#include <EEPROM.h>//
#include "DHT.h"

// Порты подключения GPRS-модема
SoftwareSerial gprsSerial(7, 8);
DHT dht(4, DHT11);

// Порт Температурного датчика
int tempPin = 1;
int sen_1 = 11;
int sen_2 = 12;
int led = 8;  //объявление переменной целого типа, содержащей номер порта к которому мы подключили второй провод
char phone[13]; // переменная для хранения массива из 20 символов для номера телефона
int addr = 0;
int secpam = 32;
int minpam = 31;
bool ledflag = false;
String tel = "";
long t1a, t1b , t1c;

float temp_dht, humid_dht, temp_lm ,tempOut;
int s1, s2;
long time_pov_ms;
char buf[50];
int min_t;
bool debugflag, gprsMessflag=false;
int minute = 0;
boolean security=true;
int minute2 = 0;
boolean min_alert=false;
int minute3=0;
bool p_s1=false; 

//
String gprs_phonenumber , gprs_command, gprs_param1, gprs_param2 = "";// номер телефона с которого приходят комманды , сами комманды,место для измения переменных

void setup()  //обязательная процедура setup, запускаемая в начале программы; объявление процедур начинается словом void

{
  // Включаем отладку
  debugflag = true; //
  gprsMessflag = true;
  p_s1=false;
  pinMode(9, OUTPUT);
  digitalWrite(9, HIGH);    // Подаем High на пин 9
  delay(3000);              // на 3 секунды
  digitalWrite(9, LOW);     // и отпускаем в Low. 
  delay(3000);             // Ждём 5 секунд для старта шилда
  dht.begin();

   // Отладка через последовательный порт
  if (debugflag)
    Serial.begin(9600);
 
  
  pinMode(led, OUTPUT); //объявление используемого порта, led - номер порта, второй аргумент - тип использования порта - на вход (INPUT) или на выход (OUTPUT)
  
  EEPROM.get(addr, phone); // считываем массив символов по адресу addr
  delay(150); 
  if (phone[1]+phone[2]+phone[3]+phone[4]+phone[5] >12){
    String telephon(phone);
    DebugText ("new tel set   ");  
    tel=telephon;
  }
  else {
    tel ="+79056897223";
  }
  
  //DebugText(tel);
  // считываем состояние охраны и минимальную температуру 
  security = EEPROM.read(secpam);
  min_t = EEPROM.read(minpam);
  //if((min_t>10)|| (min_t<2 ))
  //{
  //  EEPROM.write(31, 8);//min t
  //}

  
  gprs_init();
 
  
  t1c=0;
  t1b = 0;
  delay(500);
  if (security)
    SendMessage("Start. OXP on. Min_t="+String(min_t));// Приветственное сообщение с состоянием охраны и минимальной температурой
  else
    SendMessage("Start. OXP off. Min_t="+String(min_t));
 
  
}
void loop() //обязательная процедура loop, запускаемая циклично после процедуры setup
{
  t1a = millis();
  if (t1a-t1b > 10000) 
  {
    t1b = t1a;
    Event10sec();   
  }
  // Minute timer
  if  (t1a-t1c > 59900)
  { 
    minute3++;
    minute2++;   
    minute++;
    t1c=t1a; 
  }
  // Day timer
  if (minute >=1440)
  { 
    minute = 0; 
    EventDay();
  }
  
  delay(1000);

}
// реакция на слишком низкую температуру 
void could(int t1_sensor_value, int t2_sensor_value, int min_value)
{ 
  if (min_alert)
 {  SendMessage("minute:"+String(minute2));
    if (minute2 >= 120)// если за два часа температура по прежнему ниже минимальной то отправаем повторное сообщение
    {
      SendMessage("warning t1=" + String(t1_sensor_value)+" t2="+String(t2_sensor_value)+" min_t="+String(min_value));
      minute2 = 0;
  } 
 }
  else 
  { 
    min_alert = true;
    minute2 = 0;
    SendMessage("warning t1=" + String(t1_sensor_value)+" t2="+String(t2_sensor_value)+" min_t="+String(min_value));
  }
  
}

// Timer one day
void EventDay ()
{ if(security)
    SendMessage("temp on this day dat1: " +String(temp_dht)+ " dat2: " + String(temp_lm)+"OXP ON"); // ежедневное сообщение с температурой 2-х датчиков
   else
     SendMessage("temp on this day dat1: " +String(temp_dht)+ " dat2: " + String(temp_lm)+"OXP OFF");// и состоянием охраны
 // DebugText("OXP"+String(security));
}

// каждые 10 секунд считывается значения с датчиков
void Event10sec()
{
  ReadSensorsTemp();
  ReadSensorKeep();
  if (gprs_getMessage(true) == 1)
  {
    DebugText("Input sms:");
    DebugText(gprs_phonenumber);
    DebugText(gprs_command);
    DebugText(gprs_param1);
    DebugText(gprs_param2);
    if (gprs_command.startsWith("New_min_t"))// устанавлиет новую минимальную температуру
    {   
        int g = gprs_param1.toInt();
        //DebugText(g);
        EEPROM.write(minpam,g);
        min_t=EEPROM.read(minpam);
        SendMessage("New min_t"+ String(min_t));
    }
    if (gprs_command.startsWith("Phone"))// меняет номер телефона на тот с которого отправлена команда
    {
      EEPROM.write(addr,phone);
      tel==phone;
      SendMessage("New phone number"+ String(tel));
    }
    if (gprs_command.startsWith("Temp"))// запрашивает температуру 
    {
      EventDay();
    }
    if (gprs_command.startsWith("On"))// включает охрану
    {
      security = true;
      SendMessage("OXP on");
      EEPROM.write(secpam,security);
    } 
    if (gprs_command.startsWith("Off"))// выключает охрану
    {
      security = false;
      SendMessage("OXP off");
      EEPROM.write(secpam,security);
    }
  }
}

//void Event2hour()
//{
  
 
//}

void ReadSensorsTemp()
{
    // считывание температуру с датчиков

    humid_dht = dht.readHumidity();
    temp_dht = dht.readTemperature();
    tempOut = analogRead(tempPin);
    temp_lm = tempOut * 0.48828125;

  
    if((temp_dht<min_t)|| (temp_lm<min_t))   
      could( temp_dht,temp_lm, min_t);
      
    else
      if (min_alert)
      {
        min_alert = false;
        SendMessage("Temp in normal. Temp=" +String(temp_dht)); // если температура была ниже минимальной и стала выше минимальной , то приходит на сообщение
      }
}

void ReadSensorKeep()
{
    // считывает значения с датчиков охраны
    s1=digitalRead(sen_1);
    s2=digitalRead(sen_2);
    if ((s1==1) && (security))
      {
       AlertSecurity(s1, s2); // если один из датчиков сработал 
       p_s1=true;
       //DebugText(String(s1));
       //if (p_s1)
        // DebugText("p_s1=true");
         
      // else
        //DebugText("p_s1=false");
      }
    else      
    {
      //if (p_s1)
        //DebugText("p_s1=true1");
      //else
        //DebugText("p_s1=fals1");
      if (((p_s1)&&(s1==0))&&(security))
      {   
        p_s1=false;
        SendMessage("Everything in normal. Sen_1= " +String(s1)); // если один из датчиков сработал и потом перестал срабатывать
      }
    }
}

void AlertSecurity( int s1,int s2)// реакция на срабатывание датчика
{ if (security)
  {  
    if(minute3 >=3)
    {  
      SendMessage("Danger, thieves! sen1=" + String(s1));
      minute3=0;
    }
  }
  else
    {  
    minute3=0;
    SendMessage("Danger, thieves! sen1=" + String(s1));
    }
}


void SendMessage(String s)
{ 
  if (debugflag) // сообщения отладки
    Serial.println(s);
  
  if (gprsMessflag)
    gprs_sendmessage(tel, s);
}

// Отладочные сообщения
void DebugText(String s)
{ 
  if (debugflag)
    Serial.println(s);  
}

void gprs_init()
{
  DebugText ("gprs_init 38400");
  gprsSerial.begin(38400);
  delay(300);
  gprsSerial.println("AT+CLIP=1\r");  //включаем АОН
  delay(300);
  gprsSerial.print("AT+CMGF=1\r");//режим кодировки СМС - обычный (для англ.)
  delay(300);
  gprsSerial.print("AT+IFC=1, 1\r"); // конторль потока
  delay(300);
  gprsSerial.print("AT+CPBS=\"SM\"\r"); // включение адресной книги
  delay(300);
  gprsSerial.print("AT+CNMI=1,2,2,1,0\r");
  delay(3000);
  gprs_getMessage(false);

}

void gprs_sendmessage(String phonenumber, String s)
{
 // DebugText("before gprs_send " + phonenumber);
  gprsSerial.print("AT+CMGF=1\r");
  delay(150);    
  gprsSerial.print("AT + CMGS = \"");
  gprsSerial.print(phonenumber);
  gprsSerial.println("\"");
  delay(100);
  gprsSerial.print(s);
  gprsSerial.println((char)26);
  gprs_getMessage(false);
}

/*
  Функция приема sms. 
  Предназначена для считывания sms из очереди.
  
  Параметр phonenumber_from_only = true показывает, что вызывается проверка на sms от абонентского номера,
    а не служебная.
  Считывание служебных необходимо для корректной работы отправки sms.
  
  Возвращает зачения:
  0 - нет абонентской sms
  1 - есть sms от абонента. В таком случае глобальные переменные gprs_phonenumber, gprs_command,
    gprs_param1, gprs_param2 заполняются значениями из sms:
    gprs_phonenumber - номер телефона с которого отправилось sms
    gprs_command, gprs_param1, gprs_param2 - текст из sms, разбитый с разделителем пробел, для удобного исопльзования.
  
  Примеры использования:
    gprs_getMessage(false); // считывание служебных смс
    
    gprs_getMessage(true); // считывание абонентских sms
    в случае отправки sms с номера +79112223333 текста sms "Test 1 2"
    gprs_phonenumber = "+79112223333"
    gprs_command = "Test"
    gprs_param1 = "1"
    gprs_param2 = "2" 
*/
int gprs_getMessage(bool phonenumber_from_only)
{  
  if (gprsSerial.available()) // Если очередь не пустая
  {
    char bufGsm[64]; // buffer array for data recieve over serial port
    String inputGsmStr = ""; //входящая строка с gsm модема
    int countBufGsm = 0;
    //DebugText("available:");
    
    // Размер sms может быть большой, и возможно за один пакет не считается,
    // поэтому необходимо читать в цикле пока 
    while (gprsSerial.available()) 
    {
      //DebugText("before read");
      bufGsm[countBufGsm++] = gprsSerial.read(); // writing data into array
      if (countBufGsm == 64) // Читаем не более 64 байт
        break;
    }
    inputGsmStr += bufGsm; // Записываем символы в переменную String. на самом деле эта переменная уже не нужна, вернее нужна только для отладки.
    
    //DebugText(String(countBufGsm) + " SMS: " + inputGsmStr);
    
    if (phonenumber_from_only) 
    {
      gprs_phonenumber, gprs_command, gprs_param1, gprs_param2 = "";
      if (inputGsmStr.substring(2, 6) != "+CMT") // Условие для определния, что sms от абонентского номера
        return 0;
      else
      { 
        char space = 32; // символ-разделитель проблел
        char cmd[64];   // массив с частями sms
        int countCmd = 0; // счетчик частей sms
        
        //DebugText("Structire: ");
        gprs_phonenumber = inputGsmStr.substring(9,21);
        //DebugText(gprs_phonenumber);
        
        gprs_command = inputGsmStr.substring(50, countBufGsm - 1); // пока считываем все, потом будем разбивать
        //DebugText(gprs_command);
        
        int countPoint = 1;
        
        for (int i = 50; i < countBufGsm; i ++)
        {
          if ((bufGsm[i] == space) || (bufGsm[i] == '\n')) // условие на разделитель или конец sms
          {
            //DebugText(String(cmd));
            // Переключатель частей sms. Записываем в глобальные переменные часть sms
            switch (countPoint){
              case 1: { 
                gprs_command = cmd;
                //DebugText(cmd); 
                break; 
                }
              case 2: { 
                gprs_param1 = cmd; 
                //DebugText(cmd);
                break; 
                }
              case 3: { gprs_param2 = cmd; break; }
            }
            countPoint++;
            // Обнуляем массив частей sms
            for (int x = 0; x < countCmd; x++)
              cmd[x] = NULL;
            countCmd = 0;
          }
          else 
          {            
            cmd[countCmd++] = bufGsm[i]; // набираем массив части sms
          }
        }
        //DebugText("! " +gprs_phonenumber + gprs_command+gprs_param1+gprs_param2);
        if (gprs_command!="")
          return 1;
      }
    }
    // Обнуляем массив с буфером sms
    countBufGsm = 0; // set counter of while loop to zero    
    //DebugText("end get_gprs_message-----");
    for (int i = 0; i < countBufGsm; i++)
      bufGsm[i] = NULL;
    
  }  
  
  return 0;
}
