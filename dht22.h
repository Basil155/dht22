typedef struct DhtData
{
    int Humidity10, Temperature10;
    int Pin;
    struct DhtData *next;
} DhtData;

int DhtInit(int RmtChannel);

int DhtClose(void);

int DhtAdd(int PinNumber);

int DhtRemove(int PinNumber);

double DhtGetHumidity(int PinNumber);

double DhtGetTemperature(int PinNumber);
