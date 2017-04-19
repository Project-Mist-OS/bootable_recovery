/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agree to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include <string>
#include <vector>

#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android-base/test_utils.h>
#include <gtest/gtest.h>
#include <ziparchive/zip_archive.h>
#include <ziparchive/zip_writer.h>

#include "install.h"
#include "private/install.h"

TEST(InstallTest, verify_package_compatibility_no_entry) {
  TemporaryFile temp_file;
  FILE* zip_file = fdopen(temp_file.fd, "w");
  ZipWriter writer(zip_file);
  // The archive must have something to be opened correctly.
  ASSERT_EQ(0, writer.StartEntry("dummy_entry", 0));
  ASSERT_EQ(0, writer.FinishEntry());
  ASSERT_EQ(0, writer.Finish());
  ASSERT_EQ(0, fclose(zip_file));

  // Doesn't contain compatibility zip entry.
  ZipArchiveHandle zip;
  ASSERT_EQ(0, OpenArchive(temp_file.path, &zip));
  ASSERT_TRUE(verify_package_compatibility(zip));
  CloseArchive(zip);
}

TEST(InstallTest, verify_package_compatibility_invalid_entry) {
  TemporaryFile temp_file;
  FILE* zip_file = fdopen(temp_file.fd, "w");
  ZipWriter writer(zip_file);
  ASSERT_EQ(0, writer.StartEntry("compatibility.zip", 0));
  ASSERT_EQ(0, writer.FinishEntry());
  ASSERT_EQ(0, writer.Finish());
  ASSERT_EQ(0, fclose(zip_file));

  // Empty compatibility zip entry.
  ZipArchiveHandle zip;
  ASSERT_EQ(0, OpenArchive(temp_file.path, &zip));
  ASSERT_FALSE(verify_package_compatibility(zip));
  CloseArchive(zip);
}

TEST(InstallTest, update_binary_command_smoke) {
#ifdef AB_OTA_UPDATER
  TemporaryFile temp_file;
  FILE* zip_file = fdopen(temp_file.fd, "w");
  ZipWriter writer(zip_file);
  ASSERT_EQ(0, writer.StartEntry("payload.bin", kCompressStored));
  ASSERT_EQ(0, writer.FinishEntry());
  ASSERT_EQ(0, writer.StartEntry("payload_properties.txt", kCompressStored));
  const std::string properties = "some_properties";
  ASSERT_EQ(0, writer.WriteBytes(properties.data(), properties.size()));
  ASSERT_EQ(0, writer.FinishEntry());
  // A metadata entry is mandatory.
  ASSERT_EQ(0, writer.StartEntry("META-INF/com/android/metadata", kCompressStored));
  std::string device = android::base::GetProperty("ro.product.device", "");
  ASSERT_NE("", device);
  std::string timestamp = android::base::GetProperty("ro.build.date.utc", "");
  ASSERT_NE("", timestamp);
  std::string metadata = android::base::Join(
      std::vector<std::string>{
          "ota-type=AB", "pre-device=" + device, "post-timestamp=" + timestamp,
      },
      "\n");
  ASSERT_EQ(0, writer.WriteBytes(metadata.data(), metadata.size()));
  ASSERT_EQ(0, writer.FinishEntry());
  ASSERT_EQ(0, writer.Finish());
  ASSERT_EQ(0, fclose(zip_file));

  ZipArchiveHandle zip;
  ASSERT_EQ(0, OpenArchive(temp_file.path, &zip));
  int status_fd = 10;
  std::string path = "/path/to/update.zip";
  std::vector<std::string> cmd;
  ASSERT_EQ(0, update_binary_command(path, zip, 0, status_fd, &cmd));
  ASSERT_EQ("/sbin/update_engine_sideload", cmd[0]);
  ASSERT_EQ("--payload=file://" + path, cmd[1]);
  ASSERT_EQ("--headers=" + properties, cmd[3]);
  ASSERT_EQ("--status_fd=" + std::to_string(status_fd), cmd[4]);
  CloseArchive(zip);
#else
  // Cannot test update_binary_command() because it tries to extract update-binary to /tmp.
  GTEST_LOG_(INFO) << "Test skipped on non-A/B device.";
#endif  // AB_OTA_UPDATER
}

TEST(InstallTest, update_binary_command_invalid) {
#ifdef AB_OTA_UPDATER
  TemporaryFile temp_file;
  FILE* zip_file = fdopen(temp_file.fd, "w");
  ZipWriter writer(zip_file);
  // Missing payload_properties.txt.
  ASSERT_EQ(0, writer.StartEntry("payload.bin", kCompressStored));
  ASSERT_EQ(0, writer.FinishEntry());
  // A metadata entry is mandatory.
  ASSERT_EQ(0, writer.StartEntry("META-INF/com/android/metadata", kCompressStored));
  std::string device = android::base::GetProperty("ro.product.device", "");
  ASSERT_NE("", device);
  std::string timestamp = android::base::GetProperty("ro.build.date.utc", "");
  ASSERT_NE("", timestamp);
  std::string metadata = android::base::Join(
      std::vector<std::string>{
          "ota-type=AB", "pre-device=" + device, "post-timestamp=" + timestamp,
      },
      "\n");
  ASSERT_EQ(0, writer.WriteBytes(metadata.data(), metadata.size()));
  ASSERT_EQ(0, writer.FinishEntry());
  ASSERT_EQ(0, writer.Finish());
  ASSERT_EQ(0, fclose(zip_file));

  ZipArchiveHandle zip;
  ASSERT_EQ(0, OpenArchive(temp_file.path, &zip));
  int status_fd = 10;
  std::string path = "/path/to/update.zip";
  std::vector<std::string> cmd;
  ASSERT_EQ(INSTALL_CORRUPT, update_binary_command(path, zip, 0, status_fd, &cmd));
  CloseArchive(zip);
#else
  // Cannot test update_binary_command() because it tries to extract update-binary to /tmp.
  GTEST_LOG_(INFO) << "Test skipped on non-A/B device.";
#endif  // AB_OTA_UPDATER
}
