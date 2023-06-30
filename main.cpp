
/*
 * Title: Development of a Raspberry Pi-based Readout for Intensified Detectors
 * Authors: Bharat Chandra P, B. G. Nair , Shubham Ghatul, Mahesh Babu, Shubhangi Jain, Richa Rai, Rekhesh Mohan,Margarita Safonova and Jayant Murthy
 * Affiliations: Indian Institute of Astrophysics, Bangalore, India
 * Contact: bharat.chandra@iiap.res.in
 * Description: This code calculates XYZ using the scientific method. It takes input parameters A, B, and C, performs the necessary calculations, and outputs the results.
 * 
 * License:
 * This code is released under the MIT License.
 * 
 * Copyright (c) 2023 [Bharat Chandra]
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without *limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following *conditions:
 *
 *The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */

/*
 * Overview:
 * This code perfoms photon counting and centroiding on the image of the phosphor screen of an intensified detector. The code takes the image stream from the piped input and performs the necessary calculations. The code also transmits the centroid data to the UART port.
 */

#include <iostream>
// OpenCV libraries for debugging
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <ctime>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
//GPIO library
#include <wiringPi.h>


//Define pin map
#define EN 4
#define MODE_SELECT_PIN 5

//Define the width and height of the Video stream
#define WIDTH 1280
#define HEIGHT 720

//Declare threshold for photon event detection
//Event threshold value
#define THRESHOLD 20
//Energy threshold value
#define ENERGY_THRESHOLD_3x3 50
#define ENERGY_THRESHOLD_5x5 500

//Define the center of the phosphor screen and the radius of the phosphor screen in the image plane
#define CENX 800
#define CENY 400
#define RADIUS 300

// Modes of operation
#define MODE_3X3 0
#define MODE_5X5 1
//Set default mode to 5x5
bool Mode_select = MODE_5X5;


float x_cen[100000],y_cen[100000];
int c_max[100000],c_min[100000];
int number_of_centroids=0;

//Define the mutex for the centroid array
pthread_mutex_t cen;







unsigned char image[HEIGHT][WIDTH];
unsigned char image_cv[HEIGHT*WIDTH];

/* Thread: uart_tramitter
    * -----------------------
    *   This thread is responsible for transmitting the data to the uart port 
    *   The data is transmitted in the following format
    *   x1,y1 for 3x3 window mode
    *       x1 is the x coordinate of the centroid
    *       y1 is the y coordinate of the centroid
    *   x1,y1,c_max,c_min for 5x5 window mode 
    *       c_max is thde corner maxima
    *       c_min is the corner minima   
    *   
    * 
*/

static void* uart_transmitter(void* pUser)
{
  
    //UART Configuration
    int serial = open("/dev/serial0",O_RDWR|O_NOCTTY);
    struct termios options;
    tcgetattr(serial, &options);
    cfsetospeed(&options,B115200); // Baud rate at 155200
    tcsetattr(serial,TCSANOW,&options);
    dprintf(serial,"system on\n");
    while(1)
    {   

        float x_c[100000],y_c[100000];
        int c_max_t[100000],c_min_t[100000];
        int n_c=0;
        //get mutex lock to access the centroid array
        pthread_mutex_lock(&cen);
        while(number_of_centroids>0)
        {   
            if(n_c>100000) break;
            x_c[n_c]= x_cen[number_of_centroids];
            y_c[n_c]= y_cen[number_of_centroids];
            if(Mode_select==MODE_5X5)
            {
                c_max_t[n_c]=c_max[number_of_centroids];
                c_min_t[n_c]=c_min[number_of_centroids];
            }
            number_of_centroids--;
            n_c++;

        }
        pthread_mutex_unlock(&cen);
       //transmit the data to the uart port
        while(n_c>0)
        {
       
        if(Mode_select==MODE_5X5)
        dprintf(serial,"%f,%f,%d,%d\n",x_c[n_c],y_c[n_c],c_max_t[n_c],c_min_t[n_c]);
        else
        dprintf(serial,"%f,%f\n",x_c[n_c],y_c[n_c]);
        n_c--;
        }
    }
    close(serial);
    return 0;
}

/* Main program
    * -----------------------
    *   This program is responsible for reading the image stream from the piped input
    *   First the mode select pin is read and the mode is set accordingly
    *   The image stream is then thresholded and the centroid of eventsthe phosphor screen is calculated
    *   The centroid is then stored in the centroid array
    *   The centroid array is then accessed by the uart_transmitter thread to transmit the data to the uart port
*/

int main()
{
    fprintf(stderr, "Starting script........\n");
  
    int imgWidth = WIDTH;
    int imgHeight = HEIGHT;

    fprintf(stderr, "Camera resolution: %d x %d\n", imgWidth, imgHeight);

    FILE *fp;
    if ((fp = fopen("/dev/stdin", "rb")) == NULL)
    {
        fprintf(stderr, "Cannot open input file!\n");
        return 1;
    }
    int nRet = 0;
    pthread_t nThreadID1;
    nRet = pthread_create(&nThreadID1,NULL ,uart_transmitter ,NULL);
        if (nRet != 0)
        {
            printf("thread create failed.ret = %d\n",nRet);
            return 1;
        }
    int bufLen = imgWidth * imgHeight;
    char *buf = (char *)malloc(bufLen);
    int count = 0;
    long long framesNumber = 0;
    long long totalTime = 0;
    int col,row;
/*  Read the mode select pin and set the mode accordingly
    *   MODE_3X3: 3x3 window mode
    *   MODE_5X5: 5x5 window mode
    *
*/
    wiringPiSetup();
    pinMode(MODE_SELECT_PIN,INPUT);
    pullUpDnControl(MODE_SELECT_PIN,PUD_UP);
    if(digitalRead(MODE_SELECT_PIN)==LOW)     Mode_select=MODE_3X3;
    else    Mode_select=MODE_5X5;
    if(Mode_select==MODE_3X3)
        {
            fprintf(stderr, "Mode: 3x3\n");
            while(1)
        {   
                        //read the IMAGE stream form the piped input
                        fseek(fp, -bufLen, SEEK_END);
                        count = fread(buf, sizeof(*buf), bufLen, fp);
                        if (count == 0)
                        {
                        //print zero data
                        fprintf(stderr, "error\n");

                        break;
                        }
                   /* 
                    Convert the image stream to a 2D array for further processing
                    -partial unrolling of the loop for better performance
                    -the loop is unrolled by a factor of 8
                    */ 
                   


                    int k=0;
                        for( row=0; row < HEIGHT ;row++ )
                            {
                                for( col=0; col < WIDTH ; col+=8)
                                    {

                                    image[row][col] = *(buf+k);
                                    image[row][col+1] = *(buf+k+1);
                                    image[row][col+2] = *(buf+k+2);
                                    image[row][col+3] = *(buf+k+3);
                                    image[row][col+4] = *(buf+k+4);
                                    image[row][col+5] = *(buf+k+5);
                                    image[row][col+6] = *(buf+k+6);
                                    image[row][col+7] = *(buf+k+7);
                                    k=k+8;
                                    }
                            }

                /*
                    circular mask to remove regions outside the phosphor screen
                */
                        for( row=0; row < HEIGHT ;row++ )
                            {
                                for( col=0; col < WIDTH ; col++)
                                    {
                                        
                                        if( (row-CENY)*(row-CENY) + (col-CENX)*(col-CENX) < RADIUS*RADIUS )
                                            {
                                            image_cv[row*WIDTH+col] = image[row][col];
                                            }
                                        else
                                            {
                                            image_cv[row*WIDTH+col] = 0;
                                            image[row][col]=0;
                                            }
                                    }
                                    
                            }   
                    bool over_flow;
                    over_flow=false;
                    //variables for centroid calculation
                    int min,max,R1,R2,R3,C1,C2,C3;
                    
                        int sum;
                        pthread_mutex_lock(&cen);
                        for(int i=CENY-RADIUS;i<(CENY+RADIUS);i++)
                            {
                                    for(int j=CENX-RADIUS;j<(CENX+RADIUS);j++)
                                    {


                                            //REF: Photon Event Centroiding with UV Photon-counting Detectors J. B. Hutchings
                                            sum =0;

                                            //condition 1: center pixel should be greater than all the neighbouring 8 pixels 
                                            //condition 2: center pixel should be greater than a threshold value to ignore background variation
                                            if(image[i][j] > THRESHOLD )
                                                {
                                                if( image[i][j] > image[i-1][j-1] & image[i][j] > image[i-1][j] & image[i][j] > image[i-1][j+1] & image[i][j] > image[i][j-1]  & image[i][j] > image[i][j+1] & image[i][j] > image[i+1][j-1] & image[i][j] > image[i+1][j] & image[i][j] > image[i+1][j+1])
                                                    {
                                                    // condition 3 : Energy(sum) should be greather than a threshold
                                                    sum   =  
                                                            image[i-1][j-1] + image[i-1][j] + image[i-1][j+1] + 
                                                            image[i][j-1]   + image[i][j]   + image[i][j+1]   + 
                                                            image[i+1][j-1] + image[i+1][j] + image[i+1][j+1]  ;
                                                    if(sum > ENERGY_THRESHOLD_3x3)
                                                    {   

                                                        ;
                                                        R1= image[i-1][j-1] + image[i-1][j] + image[i-1][j+1] ;
                                                    //R2= image[i][j-1]   + image[i][j]   + image[i][j+1]   ;
                                                        R3= image[i+1][j-1] + image[i+1][j] + image[i+1][j+1] ;
                                                    
                                                    
                                                        C1= image[i-1][j-1] + image[i][j-1] + image[i+1][j-1] ;
                                                        //C2= image[i-1][j] + image[i][j] + image[i+1][j] ;
                                                        C3= image[i-1][j+1] + image[i][j+1] + image[i+1][j+1] ;
                                                        
                                                        x_cen[number_of_centroids] =(float)i + (float)(C3-C1)/((float)(sum));
                                                        y_cen[number_of_centroids] = (float)j + (float)(R3-R1)/((float)(sum));
                                                        number_of_centroids ++ ;
                                                    }
                                                    }
                                                }
                                            else continue;
                                            if(number_of_centroids > 100000) break;

                                            }
                                            if(number_of_centroids > 100000) 
                                                {break;
                                                over_flow =true;
                                                }
                            }
                        pthread_mutex_unlock(&cen);

        }
        }
    else 
        {
        fprintf(stderr, "MODE 5X5\n");
        
        while(1)
        {
                    fseek(fp, -bufLen, SEEK_END);
                    count = fread(buf, sizeof(*buf), bufLen, fp);
                    if (count == 0)
                    {
                        //print zero data
                        fprintf(stderr, "error\n");
                        break;
                    }

                   /* 
                    Convert the image stream to a 2D array for further processing
                   */
                int k=0;
                    for( row=0; row < HEIGHT ;row++ )
                        {
                            for( col=0; col < WIDTH ; col+=8)
                                {

                                image[row][col] = *(buf+k);
                                image[row][col+1] = *(buf+k+1);
                                image[row][col+2] = *(buf+k+2);
                                image[row][col+3] = *(buf+k+3);
                                image[row][col+4] = *(buf+k+4);
                                image[row][col+5] = *(buf+k+5);
                                image[row][col+6] = *(buf+k+6);
                                image[row][col+7] = *(buf+k+7);
                                k=k+8;
                                }
                        }
                /*
                    circular mask to remove regions outside the phosphor screen
                */
                    for( row=0; row < HEIGHT ;row++ )
                        {
                            for( col=0; col < WIDTH ; col++)
                                {
                                     
                                    if( (row-CENY)*(row-CENY) + (col-CENX)*(col-CENX) < RADIUS*RADIUS )
                                        {
                                        image_cv[row*WIDTH+col] = image[row][col];
                                        }
                                    else
                                        {
                                        image_cv[row*WIDTH+col] = 0;
                                        image[row][col]=0;
                                        }
                                }
                                
                        }   
                bool over_flow;
                over_flow=false;
                //variables for centroiding
                int min,max,R1,R2,R3,R4,R5,C1,C2,C3,C4,C5;
                
                    int sum;
                    pthread_mutex_lock(&cen);
                    for(int i=CENY-RADIUS;i<(CENY+RADIUS);i++)
                        {
                                for(int j=CENX-RADIUS;j<(CENX+RADIUS);j++)
                                {


                                        //REF: Photon Event Centroiding with UV Photon-counting Detectors J. B. Hutchings
                                        sum =0;

                                        //condition 1: center pixel should be greater than all the neighbouring 8 pixels 
                                        //condition 2: center pixel should be greater than a threshold value to ignore background variation
                                        if(image[i][j] > THRESHOLD )
                                            {
                                            if( image[i][j] > image[i-1][j-1] & image[i][j] > image[i-1][j] & image[i][j] > image[i-1][j+1] & image[i][j] > image[i][j-1]  & image[i][j] > image[i][j+1] & image[i][j] > image[i+1][j-1] & image[i][j] > image[i+1][j] & image[i][j] > image[i+1][j+1])
                                                {
                                                // condition 3 : Energy(sum) should be greather than a threshold
                                                sum   =  image[i-2][j-2] + image[i-2][j-1] + image[i-2][j] + image[i-2][j+1] + image[i-2][j+2] + 
                                                        image[i-1][j-2] + image[i-1][j-1] + image[i-1][j] + image[i-1][j+1] + image[i-1][j+2] +
                                                        image[i][j-2]   + image[i][j-1]   + image[i][j]   + image[i][j+1]   + image[i][j+2] + 
                                                        image[i+1][j-2] + image[i+1][j-1] + image[i+1][j] + image[i+1][j+1] + image[i+1][j+2] +  
                                                        image[i+2][j-2] + image[i+2][j-1] + image[i+2][j] + image[i+2][j+1] + image[i+2][j+2]  ;
                                                if(sum > ENERGY_THRESHOLD_5x5)
                                                {   

                                                    R1= image[i-2][j-2] + image[i-2][j-1] + image[i-2][j] + image[i-2][j+1] + image[i-2][j+2];
                                                    R2= image[i-1][j-2] + image[i-1][j-1] + image[i-1][j] + image[i-1][j+1] + image[i-1][j+2];
                                                //  R3= image[i][j-2]   + image[i][j-1]   + image[i][j]   + image[i][j+1]   + image[i][j+2];
                                                    R4= image[i+1][j-2] + image[i+1][j-1] + image[i+1][j] + image[i+1][j+1] + image[i+1][j+2];
                                                    R5= image[i+2][j-2] + image[i+2][j-1] + image[i+2][j] + image[i+2][j+1] + image[i+2][j+2];
                                                    C1= image[i-2][j-2] + image[i-1][j-2] + image[i][j-2] + image[i+1][j-2] +  image[i+2][j-2];
                                                    C2= image[i-2][j-1] + image[i-1][j-1] + image[i][j-1] + image[i+1][j-1] + image[i+2][j-1];
                                                // C3= image[i-2][j] + image[i-1][j] + image[i][j] + image[i+1][j] + image[i+2][j];
                                                    C4= image[i-2][j+1] + image[i-1][j+1] + image[i][j+1] + image[i+1][j+1] + image[i+2][j+1];
                                                    C5= image[i-2][j+2] + image[i-1][j+2] + image[i][j+2] + image[i+1][j+2] + image[i+2][j+2];
                                                    x_cen[number_of_centroids] =(float)i + (float)(2*C5+C4-C2-2*C1)/((float)(sum));
                                                    y_cen[number_of_centroids] = (float)j + (float)(2*R5+R4-R2-2*R1)/((float)(sum));
                                                    //calculate the minima and maxima value of 4 corner pixels
                                                    min = image[i-2][j-2];
                                                    max = image[i-2][j-2];
                                                    if(image[i-2][j+2] < min) min = image[i-2][j+2];
                                                    if(image[i-2][j+2] > max) max = image[i-2][j+2];
                                                    if(image[i+2][j-2] < min) min = image[i+2][j-2];
                                                    if(image[i+2][j-2] > max) max = image[i+2][j-2];
                                                    if(image[i+2][j+2] < min) min = image[i+2][j+2];
                                                    if(image[i+2][j+2] > max) max = image[i+2][j+2];
                                                    c_min[number_of_centroids] = min;
                                                    c_max[number_of_centroids] = max;
                                                    number_of_centroids ++ ;
                                                }
                                                }
                                            }
                                        else continue;
                                        if(number_of_centroids > 100000) break;

                                        }
                                        if(number_of_centroids > 100000) 
                                            {break;
                                            over_flow =true;
                                            }
                        }
                    pthread_mutex_unlock(&cen);

          }

        }
    return 0;

}

