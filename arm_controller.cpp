#include "arm_controller/arm_controller.hpp"
#include <termios.h>
#include <unistd.h>
#include <iostream>

ArmController::ArmController(): Node("arm_controller"){
    cmd_publisher_ = this->create_publisher<std_msgs::msg::String>("/cmd_arm", 10);
    goto_publisher_ = this->create_publisher<geometry_msgs::msg::Pose>("/goto", 10);

    pose_subscriber_ = this->create_subscription<geometry_msgs::msg::Pose>("/currentPose", 10, std::bind(&ArmController::poseCallback, this, std::placeholders::_1));
    laser_subscriber_ = this->create_subscription<std_msgs::msg::Float32>("/laser_data", 10, std::bind(&ArmController::laserCallback, this, std::placeholders::_1));

    readKeyboard();
}

void ArmController::laserCallback(const std_msgs::msg::Float32::SharedPtr msg) {
}

void ArmController::poseCallback(const geometry_msgs::msg::Pose::SharedPtr msg) {
    pose_actual= *msg;
    RCLCPP_INFO(this->get_logger(), "Pose llegada");
}

void ArmController::readKeyboard() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    try {
        while (rclcpp::ok()) {
            char c = getchar();
            switch (c) {
            case 'q':
                sendCommand("X+");
                break;
            case 'w':
                sendCommand("X-");
                break;
            case 'a':
                sendCommand("Y+");
                break;
            case 's':
                sendCommand("Y-");
                break;
            case 'z':
                sendCommand("Z+");
                break;
            case 'x':
                sendCommand("Z-");
                break;
            case 'h':
                sendCommand("home");
                break;
            case 'p':
                saveCurrentPose();
                break;
            case 'g':
                moveToSavedPose();
                break;
            case ' ':
                RCLCPP_INFO(this->get_logger(), "Saliendo");
	rclcpp::shutdown();
                return;
            default:
                break;
            }
        }
    } catch (...) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        throw;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

void ArmController::sendCommand(const std::string &command) {
    auto msg = std_msgs::msg::String();
    msg.data = command;
    cmd_publisher_->publish(msg);
    RCLCPP_INFO(this->get_logger(), "Moverse.");
}
void ArmController::saveCurrentPose() {
    pose_guardada=pose_actual;
    RCLCPP_INFO(this->get_logger(), "Pose guardada.");
}

void ArmController::moveToSavedPose() {
        goto_publisher_->publish(pose_guardada.value());
        RCLCPP_INFO(this->get_logger(), "Moverse a la posición.");
}


int main(int argc, char *argv[]) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ArmController>();

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
