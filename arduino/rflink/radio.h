// Radio protocol definitions

// packet type definitions (first 16 are reserved)
#define PKT_TYPE_BEACON 0x00
#define PKT_TYPE_PING   0x01
#define PKT_TYPE_PONG   0x02
#define PKT_TYPE_USER   0x10

// serial protocol error codes
#define ERR_OK          0x00    // general OK
#define ERR_PARSE       0x01    // parse error
#define ERR_PARAM       0x02    // invalid parameter
#define ERR_NO_DATA     0x03    // no data available

// node ids
#define ADDR_BROADCAST  0xFF
