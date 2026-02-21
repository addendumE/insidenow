#define MAX_PAYLOAD_SIZE 250

typedef struct {
    uint16_t len;
    uint8_t data[MAX_PAYLOAD_SIZE];
} t_payload;

extern void nowSend(const void *data, size_t len);
extern void nowInit(int channel, bool longRange, bool rxEnable);
extern bool nowReceive(t_payload *);
