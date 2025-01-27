#include "SlippiUser.h"

#ifdef _WIN32
#include "AtlBase.h"
#include "AtlConv.h"
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Version.h"

#include "Common/Common.h"
#include "Core/ConfigManager.h"

#include <codecvt>
#include <locale>

#include <json.hpp>
using json = nlohmann::json;

const std::vector<std::string> SlippiUser::default_chat_messages = {
    "ggs",
    "one more",
    "brb",
    "good luck",

    "well played",
    "that was fun",
    "thanks",
    "too good",

    "sorry",
    "my b",
    "lol",
    "wow",

    "gotta go",
    "one sec",
    "let's play again later",
    "bad connection",
};

#ifdef _WIN32
#define MAX_SYSTEM_PROGRAM (4096)
static void system_hidden(const char* cmd)
{
  PROCESS_INFORMATION p_info;
  STARTUPINFO s_info;

  memset(&s_info, 0, sizeof(s_info));
  memset(&p_info, 0, sizeof(p_info));
  s_info.cb = sizeof(s_info);

  wchar_t utf16cmd[MAX_SYSTEM_PROGRAM] = {0};
  MultiByteToWideChar(CP_UTF8, 0, cmd, -1, utf16cmd, MAX_SYSTEM_PROGRAM);
  if (CreateProcessW(NULL, utf16cmd, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &s_info, &p_info))
  {
    DWORD ExitCode;
    WaitForSingleObject(p_info.hProcess, INFINITE);
    GetExitCodeProcess(p_info.hProcess, &ExitCode);
    CloseHandle(p_info.hProcess);
    CloseHandle(p_info.hThread);
  }
}
#endif

static void RunSystemCommand(const std::string& command)
{
#ifdef _WIN32
  _wsystem(UTF8ToTStr(command).c_str());
#else
  system(command.c_str());
#endif
}

static size_t receive(char* ptr, size_t size, size_t nmemb, void* rcvBuf)
{
  size_t len = size * nmemb;
  INFO_LOG_FMT(SLIPPI_ONLINE, "[User] Received data: {}", len);

  std::string* buf = (std::string*)rcvBuf;

  buf->insert(buf->end(), ptr, ptr + len);

  return len;
}

SlippiUser::SlippiUser()
{
  CURL* curl = curl_easy_init();
  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &receive);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);

    // Set up HTTP Headers
    m_curl_header_list = curl_slist_append(m_curl_header_list, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_curl_header_list);

#ifdef _WIN32
    // ALPN support is enabled by default but requires Windows >= 8.1.
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, false);
#endif

    m_curl = curl;
  }
}

SlippiUser::~SlippiUser()
{
  // Wait for thread to terminate
  m_run_thread = false;
  if (m_file_listen_thread.joinable())
    m_file_listen_thread.join();

  if (m_curl)
  {
    curl_slist_free_all(m_curl_header_list);
    curl_easy_cleanup(m_curl);
  }
}

bool SlippiUser::AttemptLogin()
{
  std::string user_file_path = getUserFilePath();

  // TODO: Remove a couple updates after ranked
#ifndef __APPLE__
  {
#ifdef _WIN32
    std::string old_user_file_path = File::GetExeDirectory() + DIR_SEP + "user.json";
#else
    std::string old_user_file_path = File::GetUserPath(D_USER_IDX) + DIR_SEP + "user.json";
#endif
    if (File::Exists(old_user_file_path) && !File::Rename(old_user_file_path, user_file_path))
    {
      WARN_LOG_FMT(SLIPPI_ONLINE, "Could not move file {} to {}", old_user_file_path,
                   user_file_path);
    }
  }
#endif

  // Get user file
  std::string user_file_contents;
  File::ReadFileToString(user_file_path, user_file_contents);

  m_user_info = parseFile(user_file_contents);

  m_is_logged_in = !m_user_info.uid.empty();
  if (m_is_logged_in)
  {
    overwriteFromServer();
    WARN_LOG_FMT(SLIPPI_ONLINE, "Found user {} ({})", m_user_info.display_name, m_user_info.uid);
  }

  return m_is_logged_in;
}

void SlippiUser::OpenLogInPage()
{
  std::string url = "https://slippi.gg/online/enable";
  std::string path = getUserFilePath();

#ifdef _WIN32
  // On windows, sometimes the path can have backslashes and slashes mixed, convert all to
  // backslashes
  path = ReplaceAll(path, "\\", "\\");
  path = ReplaceAll(path, "/", "\\");
#endif

#ifndef __APPLE__
  char* escaped_path = curl_easy_escape(nullptr, path.c_str(), static_cast<int>(path.length()));
  path = std::string(escaped_path);
  curl_free(escaped_path);
#endif

  std::string full_url = url + "?path=" + path;

  INFO_LOG_FMT(SLIPPI_ONLINE, "[User] Login at path: {}", full_url);

#ifdef _WIN32
  std::string command = "explorer \"" + full_url + "\"";
#elif defined(__APPLE__)
  std::string command = "open \"" + full_url + "\"";
#else
  std::string command = "xdg-open \"" + full_url + "\"";  // Linux
#endif

  RunSystemCommand(command);
}

void SlippiUser::UpdateApp()
{
  std::string url = "https://slippi.gg/downloads?update=true";

#ifdef _WIN32
  std::string command = "explorer \"" + url + "\"";
#elif defined(__APPLE__)
  std::string command = "open \"" + url + "\"";
#else
  std::string command = "xdg-open \"" + url + "\"";       // Linux
#endif

  RunSystemCommand(command);
}

void SlippiUser::ListenForLogIn()
{
  if (m_run_thread)
    return;

  if (m_file_listen_thread.joinable())
    m_file_listen_thread.join();

  m_run_thread = true;
  m_file_listen_thread = std::thread(&SlippiUser::FileListenThread, this);
}

void SlippiUser::LogOut()
{
  m_run_thread = false;
  deleteFile();

  UserInfo empty_user;
  m_is_logged_in = false;
  m_user_info = empty_user;
}

void SlippiUser::OverwriteLatestVersion(std::string version)
{
  m_user_info.latest_version = version;
}

SlippiUser::UserInfo SlippiUser::GetUserInfo()
{
  return m_user_info;
}

bool SlippiUser::IsLoggedIn()
{
  return m_is_logged_in;
}

void SlippiUser::FileListenThread()
{
  while (m_run_thread)
  {
    if (AttemptLogin())
    {
      m_run_thread = false;
      break;
    }

    Common::SleepCurrentThread(500);
  }
}

// On Linux platforms, the user.json file lives in the XDG_CONFIG_HOME/SlippiOnline
// directory in order to deal with the fact that we want the configuration for AppImage
// builds to be mutable.
std::string SlippiUser::getUserFilePath()
{
#if defined(__APPLE__)
  std::string user_file_path =
      File::GetBundleDirectory() + "/Contents/Resources" + DIR_SEP + "user.json";
#else
  std::string user_file_path = File::GetUserPath(F_USERJSON_IDX);
  INFO_LOG_FMT(SLIPPI, "{}", user_file_path);
#endif
  return user_file_path;
}

inline std::string readString(json obj, std::string key)
{
  auto item = obj.find(key);
  if (item == obj.end() || item.value().is_null())
  {
    return "";
  }

  return obj[key];
}

SlippiUser::UserInfo SlippiUser::parseFile(std::string file_contents)
{
  UserInfo info;
  info.file_contents = file_contents;

  auto res = json::parse(file_contents, nullptr, false);
  if (res.is_discarded() || !res.is_object())
  {
    return info;
  }

  info.uid = readString(res, "uid");
  info.display_name = readString(res, "displayName");
  info.play_key = readString(res, "playKey");
  info.connect_code = readString(res, "connectCode");
  info.latest_version = readString(res, "latestVersion");
  info.chat_messages = SlippiUser::default_chat_messages;
  if (res["chatMessages"].is_array())
  {
    info.chat_messages = res.value("chatMessages", SlippiUser::default_chat_messages);
    if (info.chat_messages.size() != 16)
    {
      info.chat_messages = SlippiUser::default_chat_messages;
    }
  }

  return info;
}

void SlippiUser::deleteFile()
{
  std::string user_file_path = getUserFilePath();
  File::Delete(user_file_path);
}

void SlippiUser::overwriteFromServer()
{
  if (!m_curl)
    return;

  // Perform curl request
  std::string resp;
  curl_easy_setopt(m_curl, CURLOPT_URL,
                   (URL_START + "/" + m_user_info.uid + "?additionalFields=chatMessages").c_str());
  curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &resp);
  CURLcode res = curl_easy_perform(m_curl);

  if (res != 0)
  {
    ERROR_LOG_FMT(SLIPPI, "[User] Error fetching user info from server, code: {}",
                  static_cast<u8>(res));
    return;
  }

  long response_code;
  curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200)
  {
    ERROR_LOG_FMT(SLIPPI, "[User] Server responded with non-success status: {}", response_code);
    return;
  }

  // Overwrite user info with data from server
  auto r = json::parse(resp, nullptr, false);
  m_user_info.connect_code = r.value("connectCode", m_user_info.connect_code);
  m_user_info.latest_version = r.value("latestVersion", m_user_info.latest_version);
  m_user_info.display_name = r.value("displayName", m_user_info.display_name);
  if (r["chatMessages"].is_array())
  {
    m_user_info.chat_messages = r.value("chatMessages", SlippiUser::default_chat_messages);
    if (m_user_info.chat_messages.size() != 16)
    {
      m_user_info.chat_messages = SlippiUser::default_chat_messages;
    }
  }
}
