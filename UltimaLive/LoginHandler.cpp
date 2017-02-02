/* Copyright(c) 2016 UltimaLive
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "LoginHandler.h"
#include "Network\NetworkManager.h"
#include "UltimaLive.h"

#pragma region Self Registration
SelfRegisteringClass <LoginHandler> LoginHandler::m_registration;


void LoginHandler::Configure()
{
  Logger::g_pLogger->LogPrint("LoginHandler configure\n");
  UltimaLive::g_pUltimaLive->Register("LoginHandler", new LoginHandler());
}

bool LoginHandler::Initialize()
{
  Logger::g_pLogger->LogPrint("Initializing LoginHandler!\n"); 
  bool success = true;
  LoginHandler* pLoginHandler = static_cast<LoginHandler*>(UltimaLive::g_pUltimaLive->Lookup("LoginHandler"));

  if (pLoginHandler != NULL)
  {
    NetworkManager* pManager = static_cast<NetworkManager*>(UltimaLive::g_pUltimaLive->Lookup("NetworkManager"));
    
    if (pManager != NULL)
    {
      pLoginHandler->m_pManager = pManager;
      pManager->subscribeToLogout(std::bind(&LoginHandler::onLogoutRequest, pLoginHandler));
      pManager->subscribeToLoginConfirm(std::bind(&LoginHandler::onLoginConfirm, pLoginHandler, std::placeholders::_1));
      pManager->subscribeToLoginComplete(std::bind(&LoginHandler::onLoginComplete, pLoginHandler));
      pManager->subscribeToOnBeforeMapChange(std::bind(&LoginHandler::onBeforeMapChange, pLoginHandler, std::placeholders::_1));
      pManager->subscribeToServerMobileUpdate(std::bind(&LoginHandler::onServerMobileUpdate, pLoginHandler));
      pManager->subscribeToMapDefinitionUpdate(std::bind(&LoginHandler::onUpdateMapDefinitions, pLoginHandler, std::placeholders::_1));
    }
    else
    {
      success = false;
      Logger::g_pLogger->LogPrintError("LoginHandler: Failed to acquire NetworkManager Instance!\n"); 
    }
  }
  else
  {
    success = false;
    Logger::g_pLogger->LogPrintError("LoginHandler: Failed to acquire LoginHandler Instance!\n"); 
  }

  return success;
}

#pragma endregion


LoginHandler::LoginHandler()
  : m_pManager(NULL),
  m_needToSendCachedLoginPacket(false),
  m_pCachedLoginPacket(NULL),
  m_firstHashQuery(true),
  m_firstMobileUpdateFromServer(true)
{
  //do nothing
}

void LoginHandler::onUpdateMapDefinitions(std::vector<MapDefinition> definitions)
{
  m_firstMobileUpdateFromServer = true;
}

void LoginHandler::onServerMobileUpdate()
{
  if (m_firstMobileUpdateFromServer)
  {
    Logger::g_pLogger->LogPrint("First Mobile Update from the server\n");
    m_firstMobileUpdateFromServer = false;
    m_needToSendCachedLoginPacket = false;
    m_pManager->onRefreshClient();
  }
}

void LoginHandler::onBeforeMapChange(uint8_t&)
{

  Logger::g_pLogger->LogPrint("Login Handler received on before change map packet\n");

  if (m_pCachedLoginPacket != NULL && m_needToSendCachedLoginPacket)
  {
    Logger::g_pLogger->LogPrint("SENDING cached login packet\n");
    m_pManager->sendPacketToClient(m_pCachedLoginPacket);
	  m_needToSendCachedLoginPacket = false;
    m_pManager->onRefreshClient();
  }
}

void LoginHandler::onLoginConfirm(uint8_t* pPacketData)
{
  Logger::g_pLogger->LogPrint("LOGIN HANDLER RECEIVED LOGIN CONFIRM\n");
  if (m_pCachedLoginPacket == NULL)
  {
    m_pCachedLoginPacket = new uint8_t[37];
  }

  memcpy(m_pCachedLoginPacket, pPacketData, 37);
}

uint8_t* BuildUnicodeMessagePacket(uint32_t serial, uint8_t messageMode, uint16_t hue, char* message)
{
      // send welcome message
    uint8_t aUnicodeMessage[48] = { 
    /* 00          Packet ID   */    0xAE,                     
    /* 01 02       Packet Size */    0x00, 0x00,               
    /* 03 06       Serial      */    0xFF, 0xFF, 0xFF, 0xFF,   
    /* 07 08       Graphic     */    0xFF, 0xFF,               
    /* 09          Mode        */    0x03,                     
    /* 10 11       Text Color  */    0x05, 0xb2,               
    /* 12 13       Font        */    0x00, 0x03,               
    /* 14 17       Language    */    0x45, 0x4E, 0x55, 0x00,   
    /* 18 48       Name        */    0x53, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* 18 48       Name        */    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


    int messageLen = strlen(message);
    uint16_t packetLength = static_cast<uint16_t>(50 + (messageLen * 2)); // 48 + message + 2 (null terminator)
    uint8_t* pUnicodePacket = new uint8_t[packetLength];

    memset(pUnicodePacket, 0x00, packetLength);
    memcpy(pUnicodePacket, aUnicodeMessage, 48);

    *reinterpret_cast<uint16_t*>(&pUnicodePacket[1]) = packetLength;
    *reinterpret_cast<uint32_t*>(&pUnicodePacket[3]) = serial;
    pUnicodePacket[9] = messageMode;
    *reinterpret_cast<uint16_t*>(&pUnicodePacket[10]) = hue;

    for (int i = 0; i < messageLen; ++i)
    {
      pUnicodePacket[49 + (2 * i)] = message[i];
    }

    return pUnicodePacket;
}


void LoginHandler::onLoginComplete()
{
  Logger::g_pLogger->LogPrint("LOGIN HANDLER RECEIVED LOGIN COMPLETE\n");
  m_needToSendCachedLoginPacket = true;
  m_firstMobileUpdateFromServer = true;

  //send welcome message
  char welcomeMessageBuff[20];
  uint16_t minor = static_cast<uint16_t>(Utils::getModuleMinorVersionLower());
  uint16_t major = static_cast<uint16_t>(Utils::getModuleMinorVersionUpper());

  sprintf_s(welcomeMessageBuff, "ULTIMALIVE v.%u.%u", major, minor);

  uint8_t* pPacket = BuildUnicodeMessagePacket(0xFFFFFFFF, 0x03, 0xb205, welcomeMessageBuff);
  m_pManager->sendPacketToClient(pPacket);
  Logger::g_pLogger->LogPrint("%s\n", welcomeMessageBuff);
}

void LoginHandler::onLogoutRequest()
{
  Logger::g_pLogger->LogPrint("LoginHandler received logout request!\n");
  m_needToSendCachedLoginPacket = false;
  m_firstHashQuery = true;

  if (m_pCachedLoginPacket != NULL)
  {
    delete m_pCachedLoginPacket;
    m_pCachedLoginPacket = NULL;
  }
}


