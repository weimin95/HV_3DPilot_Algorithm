#include "HVUtils.h"

#include <cmath>

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

namespace {

bool IsFiniteDouble(double value) {
    return std::isfinite(value) != 0;
}

bool IsFinitePoint(const HVPoint2D& point) {
    return IsFiniteDouble(point.x) && IsFiniteDouble(point.y);
}

void MarkMaskPixel(int x, int y, cv::Mat& mask) {
    if (x < 0 || y < 0 || x >= mask.cols || y >= mask.rows) {
        return;
    }
    mask.at<unsigned char>(y, x) = 255;
}

void RasterizePoint(const HVPoint2D& point, cv::Mat& mask) {
    MarkMaskPixel(
        static_cast<int>(std::lround(point.x)),
        static_cast<int>(std::lround(point.y)),
        mask);
}

void RasterizeLine(const HVLineSegment2D& line_segment, cv::Mat& mask) {
    const cv::Point start(
        static_cast<int>(std::lround(line_segment.start_point_.x)),
        static_cast<int>(std::lround(line_segment.start_point_.y)));
    const cv::Point end(
        static_cast<int>(std::lround(line_segment.end_point_.x)),
        static_cast<int>(std::lround(line_segment.end_point_.y)));
    cv::line(mask, start, end, cv::Scalar(255), 1, cv::LINE_8);
}

void RasterizeAxisAlignedRect(const HVRect2D& rect, cv::Mat& mask) {
    for (int row = 0; row < mask.rows; ++row) {
        const double pixel_center_y = static_cast<double>(row) + 0.5;
        if (pixel_center_y < rect.y_ || pixel_center_y >= rect.y_ + rect.height_) {
            continue;
        }

        for (int col = 0; col < mask.cols; ++col) {
            const double pixel_center_x = static_cast<double>(col) + 0.5;
            if (pixel_center_x >= rect.x_ && pixel_center_x < rect.x_ + rect.width_) {
                mask.at<unsigned char>(row, col) = 255;
            }
        }
    }
}

void RasterizeRotatedRect(const HVRotatedRect2D& rect, cv::Mat& mask) {
    const double angle_rad = rect.angle_deg_ * std::acos(-1.0) / 180.0;
    const double cos_angle = std::cos(angle_rad);
    const double sin_angle = std::sin(angle_rad);
    const double half_width = rect.width_ * 0.5;
    const double half_height = rect.height_ * 0.5;
    const double sample_bias = 1e-9;

    for (int row = 0; row < mask.rows; ++row) {
        const double pixel_center_y = static_cast<double>(row) + 0.5 + sample_bias;
        for (int col = 0; col < mask.cols; ++col) {
            const double pixel_center_x = static_cast<double>(col) + 0.5 + sample_bias;
            const double dx = pixel_center_x - rect.center_.x;
            const double dy = pixel_center_y - rect.center_.y;

            // Project the pixel center back into the rectangle-local frame
            // before testing axis-aligned bounds in that local frame.
            const double local_x = dx * cos_angle + dy * sin_angle;
            const double local_y = -dx * sin_angle + dy * cos_angle;
            if (local_x >= -half_width &&
                local_x < half_width &&
                local_y >= -half_height &&
                local_y < half_height) {
                mask.at<unsigned char>(row, col) = 255;
            }
        }
    }
}

} // namespace

bool IsValidRoiInfo(const HVRoiInfo& roi) {
    if (roi.dimension_ != HVRoiDimension::ROI_2D) {
        return false;
    }

    switch (roi.shape_type_) {
    case HVRoiShapeType::Point:
        return IsFinitePoint(roi.geometry_2d_.point_);
    case HVRoiShapeType::LineSegment:
        return IsFinitePoint(roi.geometry_2d_.line_segment_.start_point_) &&
            IsFinitePoint(roi.geometry_2d_.line_segment_.end_point_);
    case HVRoiShapeType::Rectangle:
        return IsFiniteDouble(roi.geometry_2d_.rect_.x_) &&
            IsFiniteDouble(roi.geometry_2d_.rect_.y_) &&
            IsFiniteDouble(roi.geometry_2d_.rect_.width_) &&
            IsFiniteDouble(roi.geometry_2d_.rect_.height_) &&
            roi.geometry_2d_.rect_.width_ > 0.0 &&
            roi.geometry_2d_.rect_.height_ > 0.0;
    case HVRoiShapeType::RotatedRectangle:
        return IsFinitePoint(roi.geometry_2d_.rotated_rect_.center_) &&
            IsFiniteDouble(roi.geometry_2d_.rotated_rect_.width_) &&
            IsFiniteDouble(roi.geometry_2d_.rotated_rect_.height_) &&
            IsFiniteDouble(roi.geometry_2d_.rotated_rect_.angle_deg_) &&
            roi.geometry_2d_.rotated_rect_.width_ > 0.0 &&
            roi.geometry_2d_.rotated_rect_.height_ > 0.0;
    default:
        return false;
    }
}

bool BuildRoiMask(const HVRoiInfo& roi, int image_width, int image_height, cv::Mat& mask) {
    if (!IsValidRoiInfo(roi) || image_width <= 0 || image_height <= 0) {
        mask.release();
        return false;
    }

    mask = cv::Mat::zeros(image_height, image_width, CV_8UC1);
    switch (roi.shape_type_) {
    case HVRoiShapeType::Point:
        RasterizePoint(roi.geometry_2d_.point_, mask);
        return true;
    case HVRoiShapeType::LineSegment:
        RasterizeLine(roi.geometry_2d_.line_segment_, mask);
        return true;
    case HVRoiShapeType::Rectangle:
        RasterizeAxisAlignedRect(roi.geometry_2d_.rect_, mask);
        return true;
    case HVRoiShapeType::RotatedRectangle:
        RasterizeRotatedRect(roi.geometry_2d_.rotated_rect_, mask);
        return true;
    default:
        mask.release();
        return false;
    }
}

bool BuildMaskedImageFromRoi(const HVRoiInfo& roi, const ImageDataInfo2D& src_image, ImageDataInfo2D& out_image) {
    if (src_image.empty()) {
        out_image = ImageDataInfo2D();
        return false;
    }

    cv::Mat mask;
    if (!BuildRoiMask(roi, src_image.width, src_image.height, mask)) {
        out_image = ImageDataInfo2D();
        return false;
    }

    const cv::Mat src = ImageConverter::ToMat(src_image);
    cv::Mat masked = cv::Mat::zeros(src.size(), src.type());
    src.copyTo(masked, mask);
    out_image = ImageConverter::FromMat(masked);
    return true;
}
