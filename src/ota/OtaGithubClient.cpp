#include "OtaGithubClient.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

OtaGithubClient::OtaGithubClient() {}

bool OtaGithubClient::checkForUpdate(const String& owner,
                                     const String& repo,
                                     const String& currentVersion,
                                     OtaReleaseInfo& info,
                                     uint32_t timeoutMs) {
  info.hasUpdate = false;
  info.httpStatus = -1;
  info.version = "";
  info.binUrl = "";
  info.error = "";

  if (owner.length() == 0 || repo.length() == 0) {
    info.error = "Brak owner/repo";
    return false;
  }

  const String apiUrl = "https://api.github.com/repos/" + owner + "/" + repo + "/releases/latest";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, apiUrl)) {
    info.error = "HTTP begin failed";
    return false;
  }

  http.addHeader("User-Agent", "waga-wezy-esp32");
  http.addHeader("Accept", "application/vnd.github+json");
  const int code = http.GET();
  info.httpStatus = code;
  if (code != HTTP_CODE_OK) {
    info.error = "HTTP " + String(code);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  info.version = extractJsonString(payload, "tag_name");
  info.binUrl = findFirstBinUrl(payload);
  if (info.version.length() == 0 || info.binUrl.length() == 0) {
    info.error = "Brak tag_name lub bin";
    return false;
  }

  info.hasUpdate = normalizeVersion(info.version) != normalizeVersion(currentVersion);
  return true;
}

bool OtaGithubClient::installFromUrl(const String& binUrl,
                                     ProgressCallback progressCb,
                                     String& errorOut,
                                     uint32_t timeoutMs) {
  errorOut = "";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, binUrl)) {
    errorOut = "HTTP begin failed";
    return false;
  }

  http.addHeader("User-Agent", "waga-wezy-esp32");
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    errorOut = "HTTP " + String(code);
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN)) {
    errorOut = "Update.begin failed";
    http.end();
    return false;
  }

  NetworkClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  int writtenTotal = 0;
  unsigned long lastDataMs = millis();

  while (http.connected() && (contentLength < 0 || writtenTotal < contentLength)) {
    esp_task_wdt_reset();
    const size_t available = stream->available();
    if (available == 0) {
      if (millis() - lastDataMs > 10000) {
        errorOut = "Zwis pobierania (10 s bez danych)";
        Update.abort();
        http.end();
        return false;
      }
      delay(1);
      continue;
    }
    lastDataMs = millis();

    const int toRead = available > sizeof(buffer) ? sizeof(buffer) : available;
    const int readCount = stream->readBytes(buffer, toRead);
    if (readCount <= 0) {
      continue;
    }

    const size_t written = Update.write(buffer, readCount);
    if (written != (size_t)readCount) {
      errorOut = "Update.write failed";
      Update.abort();
      http.end();
      return false;
    }

    writtenTotal += readCount;
    if (progressCb != nullptr && contentLength > 0) {
      const int percent = (writtenTotal * 100) / contentLength;
      progressCb(percent > 100 ? 100 : percent);
    }
  }

  const bool ok = Update.end();
  http.end();

  if (!ok || !Update.isFinished()) {
    errorOut = "Update.end failed";
    return false;
  }

  if (progressCb != nullptr) {
    progressCb(100);
  }

  return true;
}

String OtaGithubClient::extractJsonString(const String& json, const String& keyName) {
  const String key = "\"" + keyName + "\":";
  const int keyPos = json.indexOf(key);
  if (keyPos < 0) {
    return "";
  }

  int quoteStart = json.indexOf('"', keyPos + key.length());
  if (quoteStart < 0) {
    return "";
  }
  int quoteEnd = json.indexOf('"', quoteStart + 1);
  if (quoteEnd < 0) {
    return "";
  }

  return json.substring(quoteStart + 1, quoteEnd);
}

String OtaGithubClient::findFirstBinUrl(const String& json) {
  const String marker = "\"browser_download_url\":";
  int pos = 0;
  while (true) {
    const int markerPos = json.indexOf(marker, pos);
    if (markerPos < 0) {
      return "";
    }
    const int quoteStart = json.indexOf('"', markerPos + marker.length());
    if (quoteStart < 0) {
      return "";
    }
    const int quoteEnd = json.indexOf('"', quoteStart + 1);
    if (quoteEnd < 0) {
      return "";
    }

    String url = json.substring(quoteStart + 1, quoteEnd);
    if (url.endsWith(".bin")) {
      return url;
    }
    pos = quoteEnd + 1;
  }
}

String OtaGithubClient::normalizeVersion(const String& version) {
  String out = version;
  out.trim();
  if (out.startsWith("v") || out.startsWith("V")) {
    out = out.substring(1);
  }
  return out;
}
