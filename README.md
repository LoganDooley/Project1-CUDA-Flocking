**University of Pennsylvania, CIS 5650: GPU Programming and Architecture,
Project 1 - Flocking**

* Logan Dooley
  * [LinkedIn](https://www.linkedin.com/in/logan-dooley-a205a619a/)
* Tested on: Windows 11, 13th Gen Intel(R) Core(TM) i5-13420H (2.10 GHz), 16GB RAM, RTX 4050 Laptop

## Overview

This project is a CUDA implementation of Boids, which is based off of Craig Reynold's original paper, "Flocks, Herds, and Schools: A Distributed Behavioral Model". This project also extends a base boid implementation to an application which uses the boids as an audio-visualizer for input sound files from the user. This is done by analyzing the sound waves using the cuFFT library to generate bass, mid, and high song features which are used to affect boid rules.

### Examples


## Background
A boid, a word that comes from "bird-oid", is a representation of a flocking agent which uses information about its neighbors to inform how it moves. In this implementation, each boid follows 3 rules:

| Rule | Description | Effect on Behavior |
| :---     | :---:    | :---:     |
| Cohesion | Each boid is attracted to the perceived center of its neighbors. | Stray boids will be drawn into nearby groups. |
| Separation | Each boid is repulsed from nearby boids. | Boids will not collide with one another. |
| Alignment | Each boid prefers to match the velocity of its neighbors. | Boids will move together as a herd rather than crossing paths with one another. |

## Performance Analysis

![FPS vs. Number of Boids (w/o Visualization)](images/FPSVsNumberOfBoids.png)

![FPS vs. Number of Boids (w/ Visualization)](images/FPSVsNumberOfBoidsWVisualization.png)

![FPS vs. Number of Boids for Coherent Grid](images/RVs2RComparison.png)

![FPS vs. Block Size](images/FPSVsBlockSize.png)