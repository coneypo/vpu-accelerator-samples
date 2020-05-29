#pragma once
#include <iostream>
#include <vector>
#include <string>

class ImageNetLabels {
public:
  ImageNetLabels();

  std::string imagenet_labelstring( int i );

private:
  std::vector<std::string> _labels;
};