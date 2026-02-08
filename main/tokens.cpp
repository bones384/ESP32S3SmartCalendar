//
// Created by mkowa on 08.02.2026.
//

#include "tokens.hpp"

constexpr char CLIENT_ID[] = "CLIENT_ID_HERE";
constexpr char CLIENT_SECRET[] = "CLIENT_SECRET_HERE"; //! THIS SHOULD BE ENCRYPTED AND STORED IN NVS!!!
constexpr char regenurl[] = "https://oauth2.googleapis.com/token";

bool regenTokenPair()
{

    DBG_PRINTLN("Google API connection expired or not initialised. Please enter your access code: ");
    String AUTHORIZATION_CODE;
    AUTHORIZATION_CODE.reserve(256);


    while (!Serial.available()) {delay(10);}
    while (true)
    {
        if (Serial.available()){
            char c = Serial.read();
            if (c == '\r')
            {
                break;
            }
            AUTHORIZATION_CODE+=c;
            DBG_PRINT(c);

        }
        delay(10);
    }

    DBG_PRINTLN();
    DBG_PRINT("Got code:");
    DBG_PRINTLN(AUTHORIZATION_CODE);
    DBG_PRINTLN("Token generation beginning.");
    if(!connect())
    {
        DBG_PRINTLN("WiFi connection failed - token regeneration aborted.");
        return false;
    }
    // Send get for code
    String payload;
    payload.reserve(256);
    payload = String("client_id=") + CLIENT_ID +
              "&client_secret=" + CLIENT_SECRET +
              "&redirect_uri=urn:ietf:wg:oauth:2.0:oob"
              "&code=" + AUTHORIZATION_CODE +
              "&grant_type=authorization_code";
    HTTPClient http;
    http.setReuse(false);
    http.begin(regenurl);
    DBG_PRINT("Requesting URL ... ");
    DBG_PRINT("Requesting URL .1.. ");

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(payload);
    DBG_PRINT("Requesting URL ..2. ");

    if (httpResponseCode==200) {
        DBG_PRINT("HTTP Response code: ");
        DBG_PRINTLN(httpResponseCode);
        String response = http.getString();
        http.end();
        DBG_PRINTLN(response);

        JsonDocument doc;
        if (DeserializationError error = deserializeJson(doc, response)) {
            DBG_PRINT("deserializeJson() failed: ");
            DBG_PRINTLN(error.c_str());
            http.end();
            return false;
        } else {
            // Extract values
            const char* access_token = doc["access_token"];
            const char* refresh_token = doc["refresh_token"];

            DBG_PRINT("AT: "); DBG_PRINTLN(access_token);
            DBG_PRINT("RT: "); DBG_PRINTLN(refresh_token);

            // Store in NVS
            prefs.putString("ACCESS_TOKEN",access_token);
            prefs.putString("REFRESH_TOKEN",refresh_token);
            http.end();
            return true;
        }

    }
    else {
        DBG_PRINT("Error code: ");
        DBG_PRINTLN(httpResponseCode);
    }

    // Free resources
    http.end();
    return false;

}

constexpr char refreshurl[] = "https://oauth2.googleapis.com/token";
bool refreshToken()
{
    //? If this is called, we have a valid refresh token in NVS.
    DBG_PRINTLN("Token refresh beginning.");
    if(!connect())
    {
        DBG_PRINTLN("WiFi connection failed - token refresh aborted.");
        return false;
    }
    // Send post for refresh
    String payload;
    payload.reserve(256);
    payload = String("client_id=") + CLIENT_ID +
              "&client_secret=" + CLIENT_SECRET +
              "&refresh_token=" + prefs.getString("REFRESH_TOKEN") +
              "&grant_type=refresh_token";
    HTTPClient http;
    http.setReuse(false);
    printStackUsage("before");
    http.begin(refreshurl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(payload);
    printStackUsage("afta");
    if (httpResponseCode==200) {
        DBG_PRINT("HTTP Response code: ");
        DBG_PRINTLN(httpResponseCode);
        String response = http.getString();
        http.end();
        DBG_PRINTLN(response);

        JsonDocument doc;

        if (DeserializationError error = deserializeJson(doc, response)) {
            DBG_PRINT("deserializeJson() failed: ");
            DBG_PRINTLN(error.c_str());
            http.end();
            return false;
        } else {
            // Extract values
            const char* access_token = doc["access_token"];

            DBG_PRINT("AT: "); DBG_PRINTLN(access_token);

            // Store in NVS
            prefs.putString("ACCESS_TOKEN",access_token);
            http.end();
            return true;
        }

    }
    else {
        DBG_PRINT("Error code: ");
        DBG_PRINTLN(httpResponseCode);
        DBG_PRINTLN(http.getString());
    }

    // Free resources
    http.end();
    return false;
}

String getAccessToken()
{
    //? Check if we have an access token in NVS.
    if(prefs.isKey("ACCESS_TOKEN"))
    {
        //? We do -> return it.
        return prefs.getString("ACCESS_TOKEN");
    }
    //? We don't. check if we have a refresh token in NVS.
    DBG_PRINTLN("Access token expired.");
    if(prefs.isKey("REFRESH_TOKEN"))
    {
        DBG_PRINTLN("Renewing with refresh token...");
        //? We do -> use it to generate a new access token.
        if(refreshToken()) return prefs.getString("ACCESS_TOKEN");
        //? Refresh failed!
        DBG_PRINTLN("Renewal failed! Regenerate token pair!");
        return "";
    }
    //? No token to return
    DBG_PRINTLN("No access token stored! Regenerate token pair!");
    return "";
}
