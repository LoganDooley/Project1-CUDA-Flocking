**University of Pennsylvania, CIS 5650: GPU Programming and Architecture,
Project 1 - Flocking**

* Logan Dooley
  * [LinkedIn](https://www.linkedin.com/in/logan-dooley-a205a619a/)
* Tested on: Windows 11, 13th Gen Intel(R) Core(TM) i5-13420H (2.10 GHz), 16GB RAM, RTX 4050 Laptop

## Overview

This project is a CUDA implementation of Boids, which is based off of Craig Reynold's original paper, "Flocks, Herds, and Schools: A Distributed Behavioral Model". This project also extends a base boid implementation to an application which uses the boids as an audio-visualizer for input sound files from the user. This is done by analyzing the sound waves using the cuFFT library to generate bass, mid, and high song features which are used to affect boid rules.

### Examples

#### 5000 Boids

![5k Boids](images/5kBoids.gif)

#### 100000 Boids

![100k Boids](images/100kBoids.gif)

#### Audio Visualizer (Click the image to be taken to the YouTube video)
[![Watch the video](images/AudioVisualizedBoids.png)](https://www.youtube.com/watch?v=X_MBObliUEY)

## Background
A boid, a word that comes from "bird-oid", is a representation of a flocking agent which uses information about its neighbors to inform how it moves. In this implementation, each boid follows 3 rules:

| Rule | Description | Effect on Behavior |
| :---     | :---:    | :---:     |
| Cohesion | Each boid is attracted to the perceived center of its neighbors. | Stray boids will be drawn into nearby groups. |
| Separation | Each boid is repulsed from nearby boids. | Boids will not collide with one another. |
| Alignment | Each boid prefers to match the velocity of its neighbors. | Boids will move together as a herd rather than crossing paths with one another. |

## Methodology

### Naive Method

For the naive method, each thread corresponds to a single boid. They then loop through all other boids in the simulation and do distance checks regardless. This is an O(n^2) approach to this problem.

### Uniform Grid

To optimize and reduce the number of neighbors we need to check, a common approach is to use a spatial acceleration structure called a uniform grid. In this setup we subdivide the space into cells and assign each boid to a cell. Then when we are checking for neighbors, we only have to check for neighbors in a select few surrounding cells depending on the radii of the boid rules. This provides a significant speedup over the naive approach especially with larger areas.

### Uniform Grid with Coherent Position and Velocity Arrays

The uniform grid approach results in threads needing to use a layer of indirection to map from cell -> boid index -> position + velocity index -> position + velocity. However if we sort the position and velocity arrays after binning our boids into cells, we can remove the layer of indirection from boid index -> position + velocity index to just get cell -> position + velocity index -> position + velocity. This helps reduce the total amount of global memory as well as speed up the lookup process. 

### Audio Processing

For audio processing, I used the miniaudio library for extracing and processing PCM samples, and fed those into the cuFFT library to extract frequencies. I then launched a single thread kernel to sum the bass, mid, and treble on the GPU, and used those to influence the 3 boid rules of cohesion, separation, and alignment, as well as the maximum boid speed. I also exposed an audio player interface via Dear ImGui.

## Performance Analysis

### Effect of Boid Count on FPS

For these tests, I used a block size of 128 threads per block and tested between the Naive, Uniform Grid, and Uniform Grid with Coherent Position and Velocity arrays.

Without visualizing the boids, the following graph shows the comparison:
![FPS vs. Number of Boids (w/o Visualization)](images/FPSVsNumberOfBoids.png)

Adding in visualization, we see the following:
![FPS vs. Number of Boids (w/ Visualization)](images/FPSVsNumberOfBoidsWVisualization.png)

Notice that with visualization, for the Uniform Grid, there starts to be a decline between 10k and 50k boids, and for the Coherent Uniform Grid the decline is between 50k and 100k boids. This is likely because these are thresholds in which we become bottlenecked by the boid simulation as opposed to the visualization.

#### Analysis
In the following graph of plotting the ms / Frame vs squared boid counts in the naive implementation, we see that it forms a straight line. This supports the hypothesis that the naive implementation scales as O(n^2).
![Frame Time vs. Number of Boids^2 (w/o Visualization)](images/FrameTimeVsBoidCount.png)

On the other hand, we see that with a uniform grid and a coherent uniform grid the decrease in FPS is not as quick and strays away from the O(n^2) naive implementation. There still is an n^2 term in the scaling with a uniform grid, however the constant in front of it is much smaller as we are only looking at a fixed subset of the total number of particles.

#### Effect of Coherence
We also see that the coherent uniform grid consistently performs better than its non-coherent counterpart. This is expected because having removed a layer of indirection, it reduces a global memory access per neighbor check, and global memory accesses are a large bottleneck in GPU programs.

### Effect of Cell Size on FPS

For this test, I used a block size of 128 threads per block and tested using the Uniform Grid with Coherent Position and Velocity arrays. In this case, R represents the largest radius in which boids are searching for neighbors. In the first case where cell size = R, for any boid we search all 26 neighboring cells as well as its own, but when cell size = 2R, we only search in a 2x2x2 range of 8 cells around each boid. 

![FPS vs. Number of Boids for Coherent Grid](images/RVs2RComparison.png)

For this analysis we see mostly that the case of checking 27 cells with a cell width of R is more performant than checking 8 cells with a cell width of 2R. This might be somewhat unexpected since by only checking 8 cells in the coherent grid case, we are checking 8 segments of consecutive pieces of memory, whereas for the other case we are checking 27 segments of consecutive pieces of memory. However, it is possible that the overhead of checking more segments is not as important, since grid cells are indexed in 1D firstly in X, then in Y, and finally in Z, and I check them in the same order when evaluating neighbors, a row of 3 cells in X are actually consecutive in memory. This means that we are really checking 9 segments of consecutive pieces of memory in reality. Additionally, checking a 2x2x2 region of cells with radius 2R means we are checking a total cube with side length 4R. On the other hand, checking a 3x3x3 region of cells with radius R means we are checking a total cube with side length 3R. This means that less boids have to undergo distance checks in the 27 cell checking scenario than in the 8 cell checking scenario, which can contribute to the speedup shown.

### Effect of Block Size on FPS

For these tests I used a consistent boid count of 25k across the Naive, Uniform Grid, and Uniform Grid with Coherent Position and Velocity arrays implementations. I then tested performance against power of 2 multiples of 32, the size of a warp in CUDA.

![FPS vs. Block Size](images/FPSVsBlockSize.png)

For the most part, we see that there is little impact of block size on the performance of the application. However, there is a noticable dip between 512 and 1024. This may be because of register pressure due to twice as many registers being needed with 1024 threads per block compared to 512. 

