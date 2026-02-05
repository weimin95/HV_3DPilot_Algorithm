#include "HVUtils.h"

#pragma region Image Conversion
// 将 ImageDataInfo2D 转换为 cv::Mat (深拷贝)
cv::Mat ImageConverter::ToMat(const ImageDataInfo2D& input) {
    if (input.empty()) {
        return cv::Mat();
    }

    int cv_type = GetCvTypeFromChannels(input.channels);
    cv::Mat output(input.height, input.width, cv_type);

    // 深拷贝数据
    std::memcpy(output.data, input.image_data, input.getDataSize());

    return output;
}

// 将 cv::Mat 转换为 ImageDataInfo2D (深拷贝)
ImageDataInfo2D ImageConverter::FromMat(const cv::Mat& input) {
    if (input.empty()) {
        return ImageDataInfo2D();
    }

    // 确保是连续的8位无符号数据
    if (input.depth() != CV_8U) {
        throw std::invalid_argument("Only CV_8U depth is supported");
    }

    if (!input.isContinuous()) {
        // 如果不连续，先克隆为连续数据
        cv::Mat continuous = input.clone();
        return FromMat(continuous);
    }

    int channels = input.channels();
    ImageDataInfo2D output(input.cols, input.rows, channels);

    // 深拷贝数据
    std::memcpy(output.image_data, input.data, output.getDataSize());

    return output;
}

// 将 ImageDataInfo2D 转换为 cv::Mat (零拷贝)
cv::Mat ImageConverter::ToMatShallow(ImageDataInfo2D& input) {
    if (input.empty()) {
        return cv::Mat();
    }

    int cv_type = GetCvTypeFromChannels(input.channels);

    // 创建Mat，直接使用ImageDataInfo2D的数据指针
    // 注意：不会释放数据，生命周期由ImageDataInfo2D管理
    cv::Mat output(input.height, input.width, cv_type, input.image_data);

    return output;
}

// 将 cv::Mat 转换为 ImageDataInfo2D (零拷贝)
ImageDataInfo2D ImageConverter::FromMatShallow(cv::Mat& input) {
    if (input.empty()) {
        return ImageDataInfo2D();
    }

    // 确保是连续的8位无符号数据
    if (input.depth() != CV_8U) {
        throw std::invalid_argument("Only CV_8U depth is supported");
    }

    if (!input.isContinuous()) {
        throw std::invalid_argument("Mat must be continuous for shallow copy");
    }

    ImageDataInfo2D output;
    output.width = input.cols;
    output.height = input.rows;
    output.channels = input.channels();
    output.image_data = input.data;  // 共享数据指针

    // 警告：此处不拥有数据所有权，不应调用release()
    // 数据生命周期由输入的Mat管理

    return output;
}

// 辅助函数：获取OpenCV类型的通道数
int ImageConverter::GetChannelsFromCvType(int cv_type) {
    return CV_MAT_CN(cv_type);
}

// 辅助函数：根据通道数生成OpenCV类型
int ImageConverter::GetCvTypeFromChannels(int channels) {
    switch (channels) {
    case 1: return CV_8UC1;
    case 2: return CV_8UC2;
    case 3: return CV_8UC3;
    case 4: return CV_8UC4;
    default:
        throw std::invalid_argument("Unsupported number of channels");
    }
}
#pragma endregion

#pragma region Point Cloud Conversion
/**
    * @brief 将 HVPointCloud 转换为 Open3D PointCloud
    * @param input 自定义点云
    * @return Open3D 点云智能指针
    */
std::shared_ptr<open3d::geometry::PointCloud> PointCloudConverter::ToOpen3D(const HVPointCloud& input) {
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
HVPointCloud PointCloudConverter::FromOpen3D(const open3d::geometry::PointCloud& input) {
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
pcl::PointCloud<pcl::PointXYZ>::Ptr PointCloudConverter::ToPCL(const HVPointCloud& input) {
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
HVPointCloud PointCloudConverter::FromPCL(const pcl::PointCloud<pcl::PointXYZ>& input) {
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
pcl::PointCloud<pcl::PointXYZ>::Ptr PointCloudConverter::Open3DToPCL(const open3d::geometry::PointCloud& input) {
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
std::shared_ptr<open3d::geometry::PointCloud> PointCloudConverter::PCLToOpen3D(const pcl::PointCloud<pcl::PointXYZ>& input) {
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
#pragma endregion