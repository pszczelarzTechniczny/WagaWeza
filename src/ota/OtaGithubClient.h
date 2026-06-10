#ifndef OTA_GITHUB_CLIENT_H
#define OTA_GITHUB_CLIENT_H

#include <Arduino.h>

struct OtaReleaseInfo {
  bool hasUpdate;
  int httpStatus;
  String version;
  String binUrl;
  String error;
};

class OtaGithubClient {
 public:
  typedef void (*ProgressCallback)(int percent);

  OtaGithubClient();

  bool checkForUpdate(const String& owner,
                      const String& repo,
                      const String& currentVersion,
                      OtaReleaseInfo& info,
                      uint32_t timeoutMs = 12000);

  bool installFromUrl(const String& binUrl,
                      ProgressCallback progressCb,
                      String& errorOut,
                      uint32_t timeoutMs = 30000);

 private:
  static String extractJsonString(const String& json, const String& keyName);
  static String findFirstBinUrl(const String& json);
  static String normalizeVersion(const String& version);
};

#endif
