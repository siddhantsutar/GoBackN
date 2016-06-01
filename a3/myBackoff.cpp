/*
myBackoff.cpp
Author: Siddhant Sutar (sas869)
Description: This program simulates three backoff protocols (linear, binary exponential, logarithmic) and performs latency (performance) comparison analysis.
*/

#include <iostream>
#include <fstream> 
#include <cstdlib>
#include <time.h>
#include <math.h>

using namespace std;

// Recursive backoff function that takes three arguments: type, device_count, and window_size
// type: 1 = linear, 2 = binary exponential, 3 = logarithmic
// device_count: number of devices yet to finish successful transmission
// windiw_size: current window size.
int backoff(int type, int device_count, int window_size) {
	
	int *slots = new int[window_size](); // Number of slots in the current window
	int slot_index; // Variable to store the slot index for a device
	int collisions = 0; // Number of colliding devices
	int last_device_index = 0; // Variable to store the last device index
	
	for (int i=0; i<device_count; i++) {
		slot_index = rand() % window_size; // Generate random slot index for the current device
		if (slot_index > last_device_index) { last_device_index = slot_index; } // Update last device index if required
		slots[slot_index] += 1; // Increment the count of devices in a particular slot
	}
	
	for (int i=0; i<window_size; i++) {
		if (slots[i] > 1) { // If multiple devices in a particular slot...
			collisions += slots[i]; // Increment the count of number of colliding devices
		}
	}
	
	delete slots;
	
	if (collisions) { // Recursive case: collision detection
        int latency = window_size;
		
		// Compute new window sizes depending on type
		if (type == 1) { window_size = window_size + 1; } 
		else if (type == 2) { window_size = window_size * 2; }
		else if (type == 3) { window_size = int(floor((1 + 1/(log2(window_size))) * window_size)); }
		
		return latency + backoff(type, collisions, window_size); // Recursive call
	}
	
	else { // Base case: return latency
		return last_device_index; // Time required for the last device to successfully transmit
	}

}

int main() {
	srand(time(NULL)); // Generate random seed
    ofstream out_file; // Open out_file for writing
    int total_latency; 
    for (int i=1; i<=3; i++) { // Run simulations for all three types, and open appropriate files for writing
        if (i == 1) { out_file.open("linearLatency.txt"); }
        else if (i == 2) { out_file.open("exponentialLatency.txt"); }
        else if (i == 3) { out_file.open("logarithmicLatency.txt"); }
	    for (int j=100; j<=5000; j += 100) { // Starting with 100 devices, perform simulations incrementing device count by 100 after each iteration
            total_latency = 0;
            for (int k=0; k<10; k++) { // Run 10 trials to compute average latency
                total_latency += backoff(i, j, 2); 
            }
            out_file << int(total_latency/10) << endl; // Write the average latency to the appropriate file on a newline
        }
        out_file.close(); // Close the file open for writing
    }
} 