
#include "Darknet.hpp"


Darknet::Darknet(char* cfgfile, char* wtsfile, char* metafile) {

	dn_net = load_network(cfgfile, wtsfile, 0);

	set_batch_network(dn_net, 1);  // <---  TODO:  what does this do?
	srand(2222222);

	//metadata dn_meta = get_metadata(meta_file);
}


detection* Darknet::detect(image dn_image, int *nboxes) {

	//printf("detect()\n");

	// This runs on the GPU and is the slowest bottleneck of the entire app:
	network_predict_image(dn_net, dn_image);

	detection *dets = get_network_boxes(
		dn_net, dn_image.w, dn_image.h, thresh, hier_thresh, 0, 1, nboxes
	);

	//printf(" .. nboxes: %d\n", *nboxes);

	if (nms) {
		do_nms_obj(dets, *nboxes, 1, nms);
	}

	return dets;
}

