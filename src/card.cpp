#include "card.h"
#include "display.h"
#include "Utils.h"

#if USE_DESFIRE
Desfire gi_PN532; // The class instance that communicates with Mifare Desfire cards
DESFIRE_KEY_TYPE gi_PiccMasterKey;
#else
Classic gi_PN532; // The class instance that communicates with Mifare Classic cards
#endif

// global variables
char gs8_CommandBuffer[500];  // Stores commands typed by the user via Terminal and the password
uint32_t gu32_CommandPos = 0; // Index in gs8_CommandBuffer
uint64_t gu64_LastPasswd = 0; // Timestamp when the user has enetered the password successfully
uint64_t gu64_LastID = 0;     // The last card UID that has been read by the RFID reader
bool gb_InitSuccess = false;  // true if the PN532 has been initialized successfully
kCard last_card;

void clear_kUser(kUser &user)
{
    memset(&user, 0, sizeof(kUser));
}

void clear_kCard(kCard *card)
{
    card->u8_UidLength = 0;
    card->u8_KeyVersion = 0;
    card->b_PN532_Error = false;
    card->e_CardType = CARD_Unknown; // assuming 0 is a valid value for eCardType
}

void printUnsignedCharArrayAsHex(const unsigned char *arr, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        // Print each element of the array as its hexadecimal representation
        Serial.print(arr[i], HEX);

        // Add a separator between elements (you can change this to your preference)
        Serial.print(" ");
    }

    // Move to a new line after printing the array
    Serial.println();
}

// with this card parameter and uid can be extracted and after this send to the server
//  Reads the card in the RF field.
//  In case of a Random ID card reads the real UID of the card (requires PICC authentication)
//  ATTENTION: If no card is present, this function returns true. This is not an error. (check that pk_Card->u8_UidLength > 0)
//  pk_Card->u8_KeyVersion is > 0 if a random ID card did a valid authentication with SECRET_PICC_MASTER_KEY
//  pk_Card->b_PN532_Error is set true if the error comes from the PN532.
bool ReadCard(byte u8_UID[8], kCard *pk_Card)
{
    memset(pk_Card, 0, sizeof(kCard));

    if (!gi_PN532.ReadPassiveTargetID(u8_UID, &pk_Card->u8_UidLength, &pk_Card->e_CardType))
    {
        pk_Card->b_PN532_Error = true;
        return false;
    }

    if (pk_Card->e_CardType == CARD_DesRandom) // The card is a Desfire card in random ID mode
    {
#if USE_DESFIRE
        if (!AuthenticatePICC(&pk_Card->u8_KeyVersion))
            return false;

        // replace the random ID with the real UID
        if (!gi_PN532.GetRealCardID(u8_UID))
            return false;

        pk_Card->u8_UidLength = 7; // random ID is only 4 bytes
#else
        Utils::Print("Cards with random ID are not supported in Classic mode.\r\n");
        return false;
#endif
    }
    return true;
}

// returns true if the cause of the last error was a Timeout.
// This may happen for Desfire cards when the card is too far away from the reader.
bool IsDesfireTimeout()
{
#if USE_DESFIRE
    // For more details about this error see comment of GetLastPN532Error()
    if (gi_PN532.GetLastPN532Error() == 0x01) // Timeout
    {
        Utils::Print("A Timeout mostly means that the card is too far away from the reader.\r\n");

        // In this special case we make a short pause only because someone tries to open the door
        // -> don't let him wait unnecessarily.
        // FlashLED(LED_RED, 200);
        return true;
    }
#endif
    return false;
}

// Reset the PN532 chip and initialize, set gb_InitSuccess = true on success
// If b_ShowError == true -> flash the red LED very slowly
void InitReader(bool b_ShowError)
{
    if (b_ShowError)
    {
        // SetLED(LED_RED);
        Utils::Print("Communication Error -> Reset PN532\r\n");
    }

    do // pseudo loop (just used for aborting with break;)
    {
        gb_InitSuccess = false;

        // Reset the PN532
        gi_PN532.begin(); // delay > 400 ms

        byte IC, VersionHi, VersionLo, Flags;
        if (!gi_PN532.GetFirmwareVersion(&IC, &VersionHi, &VersionLo, &Flags))
            break;

        char Buf[80];
        sprintf(Buf, "Chip: PN5%02X, Firmware version: %d.%d\r\n", IC, VersionHi, VersionLo);
        Utils::Print(Buf);
        sprintf(Buf, "Supports ISO 14443A:%s, ISO 14443B:%s, ISO 18092:%s\r\n", (Flags & 1) ? "Yes" : "No",
                (Flags & 2) ? "Yes" : "No",
                (Flags & 4) ? "Yes" : "No");
        Utils::Print(Buf);

        // Set the max number of retry attempts to read from a card.
        // This prevents us from waiting forever for a card, which is the default behaviour of the PN532.
        if (!gi_PN532.SetPassiveActivationRetries())
            break;

        // configure the PN532 to read RFID tagscustomize_card
        if (!gi_PN532.SamConfig())
            break;

        gb_InitSuccess = true;
    } while (false);

    if (b_ShowError)
    {
        Utils::DelayMilli(2000); // a long interval to make the LED flash very slowly
        // SetLED(LED_OFF);
        Utils::DelayMilli(100);
    }
}

// ================================================================================

// Modifing for sending to server
// you can call this after the server odered authenication mode
// Stores a new user and his card in the EEPROM of the Teensy
// formerly known as AddCardToEeprom
//  you get the user buffer along with the encription key from the server.
bool customize_card(const char *user_buff, const unsigned char *encript_key, unsigned char *ID)
{
    display_place_card();
    kUser k_User;
    kCard k_Card;
    if (!WaitForCard(&k_User, &k_Card))
        return false;

    // First the entire memory of s8_Name is filled with random data.
    // Then the username + terminating zero is written over it.
    // The result is for example: s8_Name[NAME_BUF_SIZE] = { 'P', 'e', 't', 'e', 'r', 0, 0xDE, 0x45, 0x70, 0x5A, 0xF9, 0x11, 0xAB }
    // The string operations like stricmp() will only read up to the terminating zero,
    // but the application master key is derived from user name + random data.
    // Utils::GenerateRandom((byte*)k_User.s8_Name, NAME_BUF_SIZE);
    // fill the name field with originally used for the name with the user buffer data, that is used to generate key and store value
    display_processing();
    strcpy(k_User.s8_Name, user_buff);

#if USE_DESFIRE
    if ((k_Card.e_CardType & CARD_Desfire) == 0) // Classic
    {
#if !ALLOW_ALSO_CLASSIC
        Utils::Print("The card is not a Desfire card.\r\n");
        return false;
#endif
    }
    else // Desfire
    {
        if (!ChangePiccMasterKey())
            return false;

        if (k_Card.e_CardType != CARD_DesRandom)
        {
            // The secret stored in a file on the card is not required when using a card with random ID
            // because obtaining the real card UID already requires the PICC master key. This is enough security.
            if (!StoreDesfireSecret(&k_User, encript_key))
            {
                Utils::Print("Could not personalize the card.\r\n");
                return false;
            }
        }
    }
#endif

    // By default a new user can open door one
    // k_User.u8_Flags = DOOR_ONE;

    // vault = k_User; // hab ich eingebaut und bin nicht stolz

    memcpy(ID, k_User.ID.u8, 7);
    Utils::Print("Customisatzion done");
    return true;
    // UserManager::StoreNewUser(&k_User);
}

// Waits for the user to approximate the card to the reader
// Timeout = 30 seconds
// Fills in pk_Card competely, but writes only the UID to pk_User.
bool WaitForCard(kUser *pk_User, kCard *pk_Card)
{
    Utils::Print("Please approximate the card to the reader now!\r\nYou have 30 seconds. Abort with ESC.\r\n");
    uint64_t u64_Start = Utils::GetMillis64();

    while (true)
    {
        if (ReadCard(pk_User->ID.u8, pk_Card) && pk_Card->u8_UidLength > 0)
        {
            // Avoid that later the door is opened for this card if the card is a long time in the RF field.
            gu64_LastID = pk_User->ID.u64;

            // All the stuff in this function takes about 2 seconds because the SPI bus speed has been throttled to 10 kHz.
            Utils::Print("Processing... (please do not remove the card)\r\n");
            return true;
        }

        if ((Utils::GetMillis64() - u64_Start) > 30000)
        {
            Utils::Print("Timeout waiting for card.\r\n");
            return false;
        }

        if (SerialClass::Read() == 27) // ESCAPE
        {
            Utils::Print("Aborted.\r\n");
            return false;
        }
    }
}

// b_PiccAuth = true if random ID card with successful authentication with SECRET_PICC_MASTER_KEY
bool authenticate_user(unsigned char *ID, char *user_buffer, kCard *pk_Card, unsigned char *key_ret)
{
    kUser k_User;

    memcpy(k_User.ID.u8, ID, 7);
    memcpy(k_User.s8_Name, user_buffer, NAME_BUF_SIZE);

    /*if (!(u64_ID == vault.ID.u64))
    {
        Utils::Print("Unknown person tries to open the door: ");
        Utils::PrintHexBuf((byte*)&u64_ID, 7, LF);
        //FlashLED(LED_RED, 1000);
        return;
    }*/

    // copyUser(vault,k_User);

#if USE_DESFIRE
    if ((pk_Card->e_CardType & CARD_Desfire) == 0) // Classic
    {
#if !ALLOW_ALSO_CLASSIC
        Utils::Print("The card is not a Desfire card.\r\n");
        // FlashLED(LED_RED, 1000);
        return false;
#endif
    }
    else // Desfire
    {
        if (pk_Card->e_CardType == CARD_DesRandom) // random ID Desfire card
        {
            // In case of a random ID card the authentication has already been done in ReadCard().
            // But ReadCard() may also authenticate with the factory default DES key, so we must check here
            // that SECRET_PICC_MASTER_KEY has been used for authentication.
            if (pk_Card->u8_KeyVersion != CARD_KEY_VERSION)
            {
                Utils::Print("The card is not personalized.\r\n");
                // FlashLED(LED_RED, 1000);
                return false;
            }
        }
        else // default Desfire card
        {
            /// unsigned char key[enc_key_length] = {0};
            if (!CheckDesfireSecret(&k_User, key_ret))
            {
                if (IsDesfireTimeout()) // Prints additional error message and blinks the red LED
                    return false;

                Utils::Print("The card is not personalized.\r\n");
                // Utils::Print("Hier ist das Problem");
                // FlashLED(LED_RED, 1000);
                return false;
            }
        }
    }
#endif

    Utils::Print("Authentic");
    // Utils::Print(k_User.s8_Name);
    switch (pk_Card->e_CardType)
    {
    case CARD_DesRandom:
        Utils::Print(" (Desfire random card)", LF);
        break;
    case CARD_Desfire:
        Utils::Print(" (Desfire default card)", LF);
        break;
    default:
        Utils::Print(" (Classic card)", LF);
        break;
    }

    // ActivateRelais(k_User.u8_Flags);

    // Avoid that the door is opened twice when the card is in the RF field for a longer time.
    gu64_LastID = k_User.ID.u64;
    return true;
}

// =================================== DESFIRE ONLY =========================================

#if USE_DESFIRE

// If the card is personalized -> authenticate with SECRET_PICC_MASTER_KEY,
// otherwise authenticate with the factory default DES key.
bool AuthenticatePICC(byte *pu8_KeyVersion)
{
    if (!gi_PN532.SelectApplication(0x000000)) // PICC level
        return false;

    if (!gi_PN532.GetKeyVersion(0, pu8_KeyVersion)) // Get version of PICC master key
        return false;

    // The factory default key has version 0, while a personalized card has key version CARD_KEY_VERSION
    if (*pu8_KeyVersion == CARD_KEY_VERSION)
    {
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    else // The card is still in factory default state
    {
        if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY))
            return false;
    }
    return true;
}

// Generate two dynamic secrets: the Application master key (AES 16 byte or DES 24 byte) and the 16 byte StoreValue.
// Both are derived from the 7 byte card UID and the the user name + random data stored in EEPROM using two 24 byte 3K3DES keys.
// This function takes only 6 milliseconds to do the cryptographic calculations.
bool GenerateDesfireSecrets(kUser *pk_User, DESFireKey *pi_AppMasterKey, byte u8_StoreValue[16])
{
    // The buffer is initialized to zero here
    byte u8_Data[24] = {0};

    // Copy the 7 byte card UID into the buffer
    memcpy(u8_Data, pk_User->ID.u8, 7);

    // XOR the user name and the random data that are stored in EEPROM over the buffer.
    // s8_Name[NAME_BUF_SIZE] contains for example { 'P', 'e', 't', 'e', 'r', 0, 0xDE, 0x45, 0x70, 0x5A, 0xF9, 0x11, 0xAB }
    int B = 0;
    for (int N = 0; N < NAME_BUF_SIZE; N++)
    {
        u8_Data[B++] ^= pk_User->s8_Name[N];
        if (B > 15)
            B = 0; // Fill the first 16 bytes of u8_Data, the rest remains zero.
    }

    byte u8_AppMasterKey[24];

    DES i_3KDes;
    if (!i_3KDes.SetKeyData(SECRET_APPLICATION_KEY, sizeof(SECRET_APPLICATION_KEY), 0) || // set a 24 byte key (168 bit)
        !i_3KDes.CryptDataCBC(CBC_SEND, KEY_ENCIPHER, u8_AppMasterKey, u8_Data, 24))
        return false;

    if (!i_3KDes.SetKeyData(SECRET_STORE_VALUE_KEY, sizeof(SECRET_STORE_VALUE_KEY), 0) || // set a 24 byte key (168 bit)
        !i_3KDes.CryptDataCBC(CBC_SEND, KEY_ENCIPHER, u8_StoreValue, u8_Data, 16))
        return false;

    // If the key is an AES key only the first 16 bytes will be used
    if (!pi_AppMasterKey->SetKeyData(u8_AppMasterKey, sizeof(u8_AppMasterKey), CARD_KEY_VERSION))
        return false;

    return true;
}

// Check that the data stored on the card is the same as the secret generated by GenerateDesfireSecrets()
// get the enctiption key from the card
bool CheckDesfireSecret(kUser *pk_User, unsigned char *enc_key)
{
    DESFIRE_KEY_TYPE i_AppMasterKey;
    byte u8_StoreValue[16];
    if (!GenerateDesfireSecrets(pk_User, &i_AppMasterKey, u8_StoreValue))
        return false;

    if (!gi_PN532.SelectApplication(0x000000)) // PICC level
        return false;

    byte u8_Version;
    if (!gi_PN532.GetKeyVersion(0, &u8_Version))
        return false;

    // The factory default key has version 0, while a personalized card has key version CARD_KEY_VERSION
    if (u8_Version != CARD_KEY_VERSION)
        return false;

    if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID))
        return false;

    if (!gi_PN532.Authenticate(0, &i_AppMasterKey))
        return false;

    // Read the 16 byte secret from the card
    byte u8_FileData[16];
    if (!gi_PN532.ReadFileData(CARD_FILE_ID, 0, 16, u8_FileData))
        return false;

    if (memcmp(u8_FileData, u8_StoreValue, 16) != 0)
        return false;

    // reading the encription key from the card and putting it into the return variable
    if (!gi_PN532.ReadFileData(CARD_FILE_ID, 16, enc_key_length, enc_key))
        return false;

    return true;
}

// Store the SECRET_PICC_MASTER_KEY on the card
bool ChangePiccMasterKey()
{
    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    if (u8_KeyVersion != CARD_KEY_VERSION) // empty card
    {
        // Store the secret PICC master key on the card.
        if (!gi_PN532.ChangeKey(0, &gi_PiccMasterKey, NULL))
            return false;

        // A key change always requires a new authentication
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }
    return true;
}

// Create the application SECRET_APPLICATION_ID,
// store the dynamic Application master key in the application,
// create a StandardDataFile SECRET_FILE_ID and store the dynamic 16 byte value into that file.
// This function requires previous authentication with PICC master key.
bool StoreDesfireSecret(kUser *pk_User, const unsigned char *enc_key)
{
    if (CARD_APPLICATION_ID == 0x000000 || CARD_KEY_VERSION == 0)
        return false; // severe errors in Secrets.h -> abort

    DESFIRE_KEY_TYPE i_AppMasterKey;
    byte u8_StoreValue[16];
    if (!GenerateDesfireSecrets(pk_User, &i_AppMasterKey, u8_StoreValue))
        return false;

    // First delete the application (The current application master key may have changed after changing the user name for that card)
    if (!gi_PN532.DeleteApplicationIfExists(CARD_APPLICATION_ID))
        return false;

    // Create the new application with default settings (we must still have permission to change the application master key later)
    if (!gi_PN532.CreateApplication(CARD_APPLICATION_ID, KS_FACTORY_DEFAULT, 1, i_AppMasterKey.GetKeyType()))
        return false;

    // After this command all the following commands will apply to the application (rather than the PICC)
    if (!gi_PN532.SelectApplication(CARD_APPLICATION_ID))
        return false;

    // Authentication with the application's master key is required
    if (!gi_PN532.Authenticate(0, &DEFAULT_APP_KEY))
        return false;

    // Change the master key of the application
    if (!gi_PN532.ChangeKey(0, &i_AppMasterKey, NULL))
        return false;

    // A key change always requires a new authentication with the new key
    if (!gi_PN532.Authenticate(0, &i_AppMasterKey))
        return false;

    // After this command the application's master key and it's settings will be frozen. They cannot be changed anymore.
    // To read or enumerate any content (files) in the application the application master key will be required.
    // Even if someone knows the PICC master key, he will neither be able to read the data in this application nor to change the app master key.
    if (!gi_PN532.ChangeKeySettings(KS_CHANGE_KEY_FROZEN))
        return false;

    // --------------------------------------------

    // Create Standard Data File with 16 bytes length
    DESFireFilePermissions k_Permis;
    k_Permis.e_ReadAccess = AR_KEY0;
    k_Permis.e_WriteAccess = AR_KEY0;
    k_Permis.e_ReadAndWriteAccess = AR_KEY0;
    k_Permis.e_ChangeAccess = AR_KEY0;
    if (!gi_PN532.CreateStdDataFile(CARD_FILE_ID, &k_Permis, (16 + enc_key_length)))
        return false;

    // Write the StoreValue into that file
    if (!gi_PN532.WriteFileData(CARD_FILE_ID, 0, 16, u8_StoreValue))
        return false;

    // Write the StoreValue into that file
    if (!gi_PN532.WriteFileData(CARD_FILE_ID, 16, enc_key_length, enc_key))
        return false;

    return true;
}

// If you have already written the master key to a card and want to use the card for another purpose
// you can restore the master key with this function. Additionally the application SECRET_APPLICATION_ID is deleted.
// If a user has been stored in the EEPROM for this card he will also be deleted.
bool RestoreDesfireCard()
{
    kUser k_User;
    kCard k_Card;
    if (!WaitForCard(&k_User, &k_Card))
        return false;

    // UserManager::DeleteUser(k_User.ID.u64, NULL);

    if ((k_Card.e_CardType & CARD_Desfire) == 0)
    {
        Utils::Print("The card is not a Desfire card.\r\n");
        return false;
    }

    byte u8_KeyVersion;
    if (!AuthenticatePICC(&u8_KeyVersion))
        return false;

    // If the key version is zero AuthenticatePICC() has already successfully authenticated with the factory default DES key
    if (u8_KeyVersion == 0)
        return true;

    // An error in DeleteApplication must not abort.
    // The key change below is more important and must always be executed.
    bool b_Success = gi_PN532.DeleteApplicationIfExists(CARD_APPLICATION_ID);
    if (!b_Success)
    {
        // After any error the card demands a new authentication
        if (!gi_PN532.Authenticate(0, &gi_PiccMasterKey))
            return false;
    }

    if (!gi_PN532.ChangeKey(0, &gi_PN532.DES2_DEFAULT_KEY, NULL))
        return false;

    // Check if the key change was successfull
    if (!gi_PN532.Authenticate(0, &gi_PN532.DES2_DEFAULT_KEY))
        return false;

    return b_Success;
}

#endif // USE_DESFIRE
