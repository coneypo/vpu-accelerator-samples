#pragma once
#include <iostream>
#include <vector>
#include <string>

/**
 * Class used to store imagenet label
 */
class ImageNetLabels {
public:
    ImageNetLabels();

    /**
     * @brief Get imagenet label by label id
     * @param i Label id
     * @return Label string
     */
    std::string imagenet_labelstring( int i );

private:
    std::vector<std::string> _labels;
};