
#include <stdio.h>
//#include <string.h>
#include <sys/time.h> // for gettimeofday
//#include <unistd.h>  // for usleep()


#include <thread>
#include <mutex>
#include <condition_variable>
#include<iostream>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "Darknet.hpp"
#include "UdpSender.hpp"



// Include files to use the PYLON API.
#include <pylon/PylonIncludes.h>
#include <pylon/FeaturePersistence.h>
// Use sstream to create image names including integer
#include <sstream>

// Namespace for using pylon objects.
using namespace Pylon;

// Namespace for using GenApi objects
using namespace GenApi;

// Namespace for using opencv objects.
using namespace cv;

// Namespace for using cout.
using namespace std;




void printTime(char* msg) {
	struct timeval tv;
	gettimeofday(&tv, 0);
	printf("%ld.%06ld %s\n", tv.tv_sec, tv.tv_usec, msg);
}



void convert_cvMat_to_image(cv::Mat mat, image dn_image) {
	int w = mat.cols;
	int h = mat.rows;
	int c = 3;  // <-- TODO: don't hard-code
	//image dn_image = make_image(w, h, c);

	int step_h = w*c;
	int step_w = c;
	int step_c = 1;

	int i, j, k;
	int index1, index2;

	for (i=0; i<h; ++i) {
		for (j=0; j<w; ++j) {
			for (k=0; k<c; ++k) {

				index1 = k*w*h + i*w + j;
				index2 = step_h*i + step_w*j + step_c*(2-k);

				dn_image.data[index1] = ((unsigned char*)mat.data)[index2]/255.;
			}
		}
	}
}


UdpSender *udpSender;

cv::Mat img_a;
cv::Mat img_a_prime;
std::mutex m;

const int pixels=500; 
const int wpixels=4;
const int hpixels=2;
const int mm=200;
const int nn=100;

int nboxes[wpixels*hpixels];
detection* dets[wpixels*hpixels]; 

image dn_image;
int current_camera_for_nn = 0;

void worker_thread(Darknet mDarknet, int nothing) {
	int oldNboxes[wpixels*hpixels];
	detection *oldDets[wpixels*hpixels];
	cv::Mat sub_image;
	while (true) {

		std::unique_lock<std::mutex> lock(m);
		
// create a copy of img_a_prime of img_a 
// because img_a is update too often
		img_a.copyTo(img_a_prime);

		cv::Point pt;
		int k=0;
		int count=0;
		// extract hpixels*wpixels subframe		
		for(int o=0;o<hpixels;o++){
			for(int l=0;l<wpixels;l++){

				k=l+o*wpixels;
				//(xa,ya) is the left-upper corner of the subframe
				int xa=l*pixels+mm;
				int ya=o*pixels+nn;
				// extract a subframe from img_a_prime
				cv::Mat sub_image_ref = img_a_prime(cv::Rect(xa ,ya,pixels,pixels));
				
				// generate sub_image independant copy of sub_image_ref.
				// otherwise darknet do not work correctly ... 
				sub_image_ref.copyTo(sub_image);
				
				convert_cvMat_to_image(sub_image, dn_image);

				int _nboxes = 0;
				// apply darknet on dn_image
				detection *_dets = mDarknet.detect(dn_image, &_nboxes);
 
 				// check how many object are detected
				for (int i=0; i< _nboxes; ++i) {
					for (int j=0; j<2; ++j) {
					
						// only select object with a prob greater than 10% accuracy
						if ( (i < _nboxes) && _dets[i].prob[j] > 0.1) {
							
							pt.x = _dets[i].bbox.x *pixels;
							pt.y = _dets[i].bbox.y *pixels ;
							//cout<<pt.x<<" "<<pt.y<<endl;
							count++;
							if (j == 0) {
								cv::circle(sub_image_ref, pt, 10, cv::Scalar(0,255,0), 3);
							} 

						}
			
					}
           		}
				// Create a rectangle around the subframe on img_a_prime
				Rect r=Rect(xa ,ya,pixels,pixels);
				cv::rectangle(img_a_prime,r , cv::Scalar(255,0,0), 3,8,0);

				// I follow Mark code here ... not sure what it is the use 
				oldNboxes[k] = nboxes[k];
				oldDets[k] = dets[k]; 

				nboxes[k] = _nboxes;
				dets[k] = _dets;

			}
		}
		
		cv::imshow("Darknet_test 2", img_a_prime);
		int ch = cv::waitKey(1);
 		cout<<"Nuts detected: "<<count<<endl;
 
 		// Send the data
		udpSender->sendData(
			current_camera_for_nn,
			dets,
			nboxes
		);

		// free the memory allocated by darknet
		// using oldDets and oldNboxes (why not using dets and nbodes ?
		for (int k=0; k< wpixels*hpixels; ++k) {	
			free_detections(oldDets[k], oldNboxes[k]);
		}
		
		// DO NOT free_image here.  The image is allocated once, and re-used
		// over and over.
		//    DON'T DO:  free_image(dn_image);

	}
}

// use a video instead of the basler
int main_video(int argc, char* argv[]) {
	printf("Yo.\n");

	if (argc != 4) {
		printf("Usage: \n");
		printf(" %s path/to/neuralNet.cfg path/to/neuralNet.weights path/to/pylonviewer/features.pfs\n",
			argv[0]);

		return -1;
	}
 
 
	// Create the UdpSender object for data sending 
	udpSender = new UdpSender();

	// Open the video stream
	VideoCapture cap("/home/nvidia/drivers/dualCameraNN/Darknet-Nuts5/lane3_tree1Basler_acA2500-20gc_(40005735)_20190305_154859995.avi");
	
	Mat img_display;
	// Get the first frame in orer to define the width an height
	cap>>img_display;

	// The video are save upside down ... Yamaguchi special :)
	// For the rover it may need to be commented
	flip(img_display,img_display,-1);

	// copy the current frame to img_a (global variable)
	img_display.copyTo(img_a);
	
	//dn_image = make_image(img_a.cols,img_a.rows , 3);
		dn_image = make_image(pixels, pixels, 3);

	//Load weight for darknet
	Darknet mDarknet(argv[1], argv[2], (char*)"nothingYet");
	
	//Run darknet on a separated thread
	std::thread worker(worker_thread, mDarknet, 5);

	while(true){
		Mat img_display;
		//Capture a frame from video
		cap>>img_display;
		// Check if a new frame is available 
		// otherwise break while loop and exit
		if(img_display.empty())
			break;

		// Yamaguchi special			
		flip(img_display,img_display,-1);
		
		// copy to frame to global img_a
		img_display.copyTo(img_a);
		
		// increase the time to 500-800
		// for matching darknet speed 
		int ch = cv::waitKey(1);

	}
	// Release VideoCapture
	cap.release();

}


int main(int argc, char* argv[]) {

	if (argc != 4) {
		printf("Usage: \n");
		printf(" %s NN.cfg NN.weights pylonviewer_features.pfs\n",argv[0]);
		
		return -1;
	}
 
 
 
	udpSender = new UdpSender();

	// Capture parameters for camera
  // =============================
  // The exit code of the sample application.
  int exitCode = 0;

  // Automagically call PylonInitialize and PylonTerminate
  // to ensure the pylon runtime system
  // is initialized during the lifetime of this object.
	Pylon::PylonAutoInitTerm autoInitTerm; 


	try{
		// Create an instant camera object with the camera device found first.
		cout << "Creating Camera..." << endl;
		CInstantCamera camera(CTlFactory::GetInstance().CreateFirstDevice());
 
    // Print the model name of the camera.
		cout << "Camera Created." << endl;
    cout << "Camera model name: " << camera.GetDeviceInfo().GetModelName() << endl;

		// The parameter MaxNumBuffer can be used to control the count of buffers
		// allocated for grabbing. The default value of this parameter is 10.
		camera.MaxNumBuffer = 1;
		INodeMap& nodemap= camera.GetNodeMap();

		// Configuration of the camera using the pfs file made with pylon viewer
		camera.Open();
 		Pylon::CFeaturePersistence::Load(argv[3],&camera.GetNodeMap(),true);

		// create pylon image format converter and pylon image
		CImageFormatConverter formatConverter;
		formatConverter.OutputPixelFormat= PixelType_BGR8packed;
		CPylonImage pylonImage;
    // The camera device is parameterized with a default configuration which
    // sets up free-running continuous acquisition.
		// StartGrabbing(num), num = number of image to grab in total
    camera.StartGrabbing(GrabStrategy_LatestImageOnly);

		// This pointer receive the grab data.
		CGrabResultPtr ptrGrabResult;

		camera.RetrieveResult( 1500, ptrGrabResult, TimeoutHandling_ThrowException);
		// Camera data size; Width and height;
		int new_width =  ptrGrabResult->GetWidth();
		int new_height=  ptrGrabResult->GetHeight();
		int _RGB_WIDTH = new_width;
		int _RGB_HEIGHT = new_height;

		// Darknet parameters
		Darknet mDarknet(argv[1], argv[2], (char*)"nothingYet");

		//dn_image = make_image(new_width, new_height, 3);
		dn_image = make_image(pixels, pixels, 3);
		std::thread worker(worker_thread, mDarknet, 5);
 
		cout<< "Frame size D"<< new_height<<endl;
		cout<< "Frame size w "<< new_width<<endl;

		// Create an OpenCV image 
		cv::Mat img_display;  
		while ( camera.IsGrabbing()){
			// Wait for an image. A timeout of 1s (1000ms) is used.
			camera.RetrieveResult( 1500, ptrGrabResult, TimeoutHandling_ThrowException);
	
			// Image grabbed successfully?
			if (ptrGrabResult->GrabSucceeded()){
				// Get image.
				const uint8_t *pImageBuffer = (uint8_t *) ptrGrabResult->GetBuffer();

				// Convert the grabbed buffer to pylon imag
				formatConverter.Convert(pylonImage, ptrGrabResult);
				// Create an OpenCV image out of pylon image
				img_display= cv::Mat(ptrGrabResult->GetHeight(), ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t *) pylonImage.GetBuffer());
				img_display.copyTo(img_a);
/*
				// Mark code
				// Tried to change as less as possible
				//------------------------------------

				cv::Point pt;
				int count=0;
				// loop for right and left camera
				//for (int camNo = 0; camNo<1; ++camNo) {
				int camNo=0;
				// Loop for object detected
				//for (int j=0; j<dn_meta.classes; ++j) {
				for (int i=0; i< nboxes; ++i) {
					for (int j=0; j<2; ++j) {
						//int j = 0;  // class #0 is "person", the only category we care about
						// TODO:  race condition between i and nboxes b/c of the
						// other thread.  Need to fix this properly...

						if ( (i < nboxes) && dets[i].prob[j] > 0.10) {
							count++;
							pt.x = dets[i].bbox.x * img_a.cols + _RGB_WIDTH*camNo;
							pt.y = dets[i].bbox.y * img_a.rows;

							if (j == 0) {
								cv::circle(img_display, pt, 10, cv::Scalar(0,0,255), 3);
							} 
							else {
								cv::circle(img_display, pt, 10, cv::Scalar(0,255,0), 3);
							}
						}
					}
				}

				namedWindow( "Darknet", CV_WINDOW_NORMAL);//AUTOSIZE //FREERATIO
				cv::imshow("Darknet", img_display);
 
				int ch = cv::waitKey(1);
				// ---------------------------------------------------------
				//  BEGIN:  Handle user closing the window
				// ---------------------------------------------------------
				switch (ch) {
					case 27: // esc key
					case 81:  // "Q"
					case 113:  // "q"
					camera.StopGrabbing();
					destroyAllWindows();
					goto theEnd;
				}
 
				int res = cv::getWindowProperty("Darknet", 1);
				if (res < 0) { // window doesn't exist (because user closed it)
					camera.StopGrabbing();
					goto theEnd;
				}
*/
				// ---------------------------------------------------------
				//  END:  Handle user closing the window
				// ---------------------------------------------------------

				ptrGrabResult.Release();
 			}
			else{
				cout << "Error: " << ptrGrabResult->GetErrorCode() << " " << ptrGrabResult->GetErrorDescription() << endl;
    	}
		}
		camera.StopGrabbing();
		destroyAllWindows();
	}
	catch (GenICam::GenericException &e){
		// Error handling.
		cerr << "An exception occurred." << endl
		<< e.GetDescription() << endl;
		exitCode = 1;
	}

	theEnd:

	return 0;
}

