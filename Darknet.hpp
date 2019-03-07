#ifndef __DARKNET_HPP_
#define __DARKNET_HPP_

extern "C" {
#include <darknet.h>
}


class Darknet {
private:
	float thresh = 0.35;
	float hier_thresh = 0.5;
	float nms = 0.45;
	network *dn_net;

public:
	Darknet(char*, char*, char*);
	detection *detect(image, int*);
};


#endif // __DARKNET_HPP_

