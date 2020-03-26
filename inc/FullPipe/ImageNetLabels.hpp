#pragma once
#include <iostream>
#include <vector>
#include <string>

#include "ImageNetLabels.hpp"


class ImageNetLabels {
public:
  ImageNetLabels();

  std::string imagenet_labelstring( int i );

private:
  std::vector<std::string> _labels;

};