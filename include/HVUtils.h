#pragma once

#include "3d_pilot_public_def.h"
#include <opencv2/opencv.hpp>
#include <open3d/Open3D.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

cv::Mat wrapToMat(const ImageDataInfo2D& info)
{
    int type = CV_8UC1;
    if (info.channels == 3)
        type = CV_8UC3;
    else if (info.channels == 4)
        type = CV_8UC4;

    return cv::Mat(
        static_cast<int>(info.height),
        static_cast<int>(info.width),
        type,
        info.image_data
    );
}

ImageDataInfo2D matToImageData(const cv::Mat& mat)
{
    ImageDataInfo2D out;
    out.width = mat.cols;
    out.height = mat.rows;
    out.channels = mat.channels();

    size_t bytes = mat.total() * mat.elemSize();
    out.image_data = new char[bytes];
    std::memcpy(out.image_data, mat.data, bytes);

    return out;
}

// HVPointCloud转Open3D/PCL点云
class PointCloudConverter {
public:
    /**
     * @brief 将 HVPointCloud 转换为 Open3D PointCloud
     * @param input 自定义点云
     * @return Open3D 点云智能指针
     */
    static std::shared_ptr<open3d::geometry::PointCloud> ToOpen3D(const HVPointCloud& input) {
        auto output = std::make_shared<open3d::geometry::PointCloud>();
        // 1. 预分配内存
        size_t num_points = input.points.size();
        output->points_.resize(num_points);
        // 2. 逐个赋值
        for (size_t i = 0; i < num_points; ++i) {
            output->points_[i] = Eigen::Vector3d(
                input.points[i].x,
                input.points[i].y,
                input.points[i].z
            );
        }
        return output;
    }

    /**
     * @brief 将 Open3D PointCloud 转换为 HVPointCloud
     * @param input Open3D 点云
     * @return 自定义点云结构
     */
    static HVPointCloud FromOpen3D(const open3d::geometry::PointCloud& input) {
        HVPointCloud output;
        // 1. 预分配内存
        size_t num_points = input.points_.size();
        output.points.resize(num_points);
        // 2. 逐个赋值
        for (size_t i = 0; i < num_points; ++i) {
            const auto& pt = input.points_[i]; // Eigen::Vector3d
            output.points[i].x = pt.x();
            output.points[i].y = pt.y();
            output.points[i].z = pt.z();
        }
        return output;
    }

    /**
     * @brief 将 HVPointCloud 转换为 PCL PointCloud
     * @param input 自定义点云
     * @return PCL 点云智能指针
     */
    static pcl::PointCloud<pcl::PointXYZ>::Ptr ToPCL(const HVPointCloud& input) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr output(new pcl::PointCloud<pcl::PointXYZ>);
        // 1. 预分配内存
        size_t num_points = input.points.size();
        output->points.resize(num_points);
        output->width = num_points;
        output->height = 1;
        output->is_dense = false;
        // 2. 逐个赋值
        for (size_t i = 0; i < num_points; ++i) {
            output->points[i].x = input.points[i].x;
            output->points[i].y = input.points[i].y;
            output->points[i].z = input.points[i].z;
        }
        return output;
    }

    /**
     * @brief 将 PCL PointCloud 转换为 HVPointCloud
     * @param input PCL 点云
     * @return 自定义点云结构
     */
    static HVPointCloud FromPCL(const pcl::PointCloud<pcl::PointXYZ>& input) {
        HVPointCloud output;
        // 1. 预分配内存
        size_t num_points = input.points.size();
        output.points.resize(num_points);
        // 2. 逐个赋值
        for (size_t i = 0; i < num_points; ++i) {
            output.points[i].x = input.points[i].x;
            output.points[i].y = input.points[i].y;
            output.points[i].z = input.points[i].z;
        }
        return output;
    }

    /**
     * @brief 将 Open3D PointCloud 转换为 PCL PointCloud
     * @param input Open3D 点云
     * @return PCL 点云智能指针
     */
    static pcl::PointCloud<pcl::PointXYZ>::Ptr Open3DToPCL(const open3d::geometry::PointCloud& input) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr output(new pcl::PointCloud<pcl::PointXYZ>);
        size_t num_points = input.points_.size();
        output->points.resize(num_points);
        output->width = num_points;
        output->height = 1;
        output->is_dense = false;

        for (size_t i = 0; i < num_points; ++i) {
            const auto& pt = input.points_[i];
            output->points[i].x = pt.x();
            output->points[i].y = pt.y();
            output->points[i].z = pt.z();
        }
        return output;
    }

    /**
     * @brief 将 PCL PointCloud 转换为 Open3D PointCloud
     * @param input PCL 点云
     * @return Open3D 点云智能指针
     */
    static std::shared_ptr<open3d::geometry::PointCloud> PCLToOpen3D(const pcl::PointCloud<pcl::PointXYZ>& input) {
        auto output = std::make_shared<open3d::geometry::PointCloud>();
        size_t num_points = input.points.size();
        output->points_.resize(num_points);

        for (size_t i = 0; i < num_points; ++i) {
            output->points_[i] = Eigen::Vector3d(
                input.points[i].x,
                input.points[i].y,
                input.points[i].z
            );
        }
        return output;
    }
};