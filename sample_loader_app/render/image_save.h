#ifndef LOADER_APP_IMAGE_SAVE_H
#define LOADER_APP_IMAGE_SAVE_H

#include <vector>
#include <string>
#include <cstdint>

bool saveImageLDR(const std::string& a_filename, const std::vector<unsigned char> &a_data,
                  int width, int height, int channels);

bool saveImageLDR(const std::string& a_filename, const std::vector<uint32_t> &a_data,
                  int width, int height, int channels);

#endif //LOADER_APP_IMAGE_SAVE_H
