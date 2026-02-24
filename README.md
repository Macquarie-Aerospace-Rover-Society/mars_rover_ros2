# mars_rover_ros2



This is the main repo for controlling the MARS Society Rover
<img width="640" height="498" alt="image" src="https://github.com/user-attachments/assets/45e4c0a2-aca2-4b7b-8e4c-39f62734ea14" />




## Installation

```bash
mkdir mars_ws/src -p
cd mars_ws/src
git clone https://github.com/DARKMOONlite/mars_rover_ros2.git
cd ../..
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install

```

## Viewing the Rover

```bash
ros2 launch mars_rover view_rover.launch.py 
```


## Running 
```bash
ros2 launch mars_rover rover.launch.py
```
- run on the onboard jetson
```bash
ros2 launch mars_rover controller.launch.py #joy_con:={xbox/ps3/joy-con}
```
- This runs on the base station to translate controller input into `Twist` msgs for the rover to interpret.
