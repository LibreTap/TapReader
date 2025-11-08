#pragma once

#include <Arduino.h>
#include "config.h"
#include "PN532.h"

#if USE_DESFIRE
#if USE_AES
#define DESFIRE_KEY_TYPE AES
#define DEFAULT_APP_KEY gi_PN532.AES_DEFAULT_KEY
#else
#define DESFIRE_KEY_TYPE DES
#define DEFAULT_APP_KEY gi_PN532.DES3_DEFAULT_KEY
#endif
#include "Desfire.h"
#include "Secrets.h"
#include "Buffer.h"
#else
#include "Classic.h"
#endif

struct kCard
{
    byte u8_UidLength;  // UID = 4 or 7 bytes
    byte u8_KeyVersion; // for Desfire random ID cards
    bool b_PN532_Error; // true -> the error comes from the PN532, false -> crypto error
    eCardType e_CardType;
};

// user structure
struct kUser
{
    // Constructor
    kUser()
    {
        memset(this, 0, sizeof(kUser));
    }

    // Card ID (4 or 7 bytes), binary
    union
    {
        uint64_t u64;
        byte u8[8];
    } ID;

    // User name (plain text + terminating zero character) + appended random data if name is shorter than NAME_BUF_SIZE
    char s8_Name[NAME_BUF_SIZE];
};

void InitReader(bool b_ShowError);

// Card state functions
void clear_kUser(kUser &user);
void clear_kCard(kCard *card);
void printUnsignedCharArrayAsHex(const unsigned char *arr, size_t size);

// Card operations
bool ReadCard(byte u8_UID[8], kCard *pk_Card);
bool WaitForCard(kUser *pk_User, kCard *pk_Card);
bool customize_card(const char *user_buff, const unsigned char *encript_key, unsigned char *ID);
bool authenticate_user(unsigned char *ID, char *user_buffer, kCard *pk_Card, unsigned char *key_ret);
bool IsDesfireTimeout();

// DESFire-specific (if needed)
bool AuthenticatePICC(byte *pu8_KeyVersion);
bool GenerateDesfireSecrets(kUser *pk_User, DESFireKey *pi_AppMasterKey, byte u8_StoreValue[16]);
bool CheckDesfireSecret(kUser *pk_User, unsigned char *enc_key);
bool ChangePiccMasterKey();
bool StoreDesfireSecret(kUser *pk_User, const unsigned char *enc_key);
bool RestoreDesfireCard();

// Card-related globals (declare as extern if needed)
extern kCard last_card;
extern unsigned char id_vault[7];

extern Desfire gi_PN532; // or Classic gi_PN532, depending on your config
extern DESFIRE_KEY_TYPE gi_PiccMasterKey;

extern char gs8_CommandBuffer[500];
extern uint32_t gu32_CommandPos;
extern uint64_t gu64_LastPasswd;
extern uint64_t gu64_LastID;
extern bool gb_InitSuccess;
