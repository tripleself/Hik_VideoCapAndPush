#include <iostream>
#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "infer.h"
#include "tracker.h"
#include "counting_line.h"
#include "config.h"

// �򵥵�Ŀ����ʾ��
void simpleDetectionExample()
{
    std::cout << "=== ��Ŀ����ʾ�� ===" << std::endl;

    // ��ʼ�������
    std::string engine_path = "./models/yolo11n.plan";
    YoloDetector detector(engine_path, 0, 0.45f, 0.25f, 80);

    // ���ز���ͼ��
    cv::Mat img = cv::imread("./test_image.jpg");
    if (img.empty())
    {
        std::cout << "�޷����ز���ͼ��" << std::endl;
        return;
    }

    // ִ�м��
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<Detection> detections = detector.inference(img);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "���ʱ��: " << duration.count() << " ms" << std::endl;
    std::cout << "��⵽ " << detections.size() << " ��Ŀ��" << std::endl;

    // ���ƽ��
    YoloDetector::draw_image(img, detections);

    // ������
    cv::imwrite("./detection_result.jpg", img);
    std::cout << "������ѱ��浽: detection_result.jpg" << std::endl;
}

// ��Ƶ׷��ʾ�������������߼�����
void videoTrackingExample()
{
    std::cout << "\n=== ��ƵĿ��׷��ʾ�������������߼����� ===" << std::endl;

    // ��ʼ���������׷����
    std::string engine_path = "./models/yolo11n.plan";
    YoloDetector detector(engine_path, 0, 0.45f, 0.25f, 80);
    TrackerModule tracker(30, 30, false, 0); // 30fps, 30֡����, ��ʹ��ReID, ׷����Ա

    // ����Ƶ�ļ�
    cv::VideoCapture cap("./test_video.mp4");
    if (!cap.isOpened())
    {
        std::cout << "�޷�����Ƶ�ļ���" << std::endl;
        return;
    }

    // ��ȡ��Ƶ��Ϣ
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);

    std::cout << "��Ƶ��Ϣ: " << frame_width << "x" << frame_height
              << ", " << fps << " fps" << std::endl;

    // ��ʼ������ģ��
    CountingLineModule counting_module(frame_width, frame_height, fps, "test_video.mp4");

    // ���������Ƶ
    cv::VideoWriter writer("./tracking_result_with_counting.mp4",
                           cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
                           fps, cv::Size(frame_width, frame_height));

    if (!writer.isOpened())
    {
        std::cout << "�޷����������Ƶ�ļ���" << std::endl;
        return;
    }

    // ���ü�����¼�ļ�����������
    std::string count_file = "counting_results_example.txt";
    counting_module.setCountingFile(count_file);
    counting_module.startCounting();

    cv::Mat frame;
    int frame_count = 0;
    double total_detect_time = 0, total_track_time = 0;

    while (cap.read(frame))
    {
        frame_count++;
        auto frame_start_time = std::chrono::high_resolution_clock::now();

        // ���㵱ǰ֡��ʱ���
        double current_frame_time_ms = (frame_count - 1) * (1000.0 / fps);

        // ִ�м��
        auto detect_start = std::chrono::high_resolution_clock::now();
        std::vector<Detection> detections = detector.inference(frame);
        auto detect_end = std::chrono::high_resolution_clock::now();

        // ִ��׷��
        auto track_start = std::chrono::high_resolution_clock::now();
        std::vector<TrackResult> tracks = tracker.update(detections);
        auto track_end = std::chrono::high_resolution_clock::now();

        // ���㴦��ʱ��
        auto detect_time = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
        auto track_time = std::chrono::duration_cast<std::chrono::milliseconds>(track_end - track_start);

        total_detect_time += detect_time.count();
        total_track_time += track_time.count();

        // ������ʵ����ʱ��
        auto frame_end_time = std::chrono::high_resolution_clock::now();
        double real_processing_time_ms = std::chrono::duration<double, std::milli>(frame_end_time - frame_start_time).count();

        // ʹ�ü���ģ����м������
        int new_crossings = counting_module.updateCounting(tracks, current_frame_time_ms, real_processing_time_ms);

        // ������������
        counting_module.drawDetectionLine(frame);

        // ����׷�ٽ��
        TrackerModule::drawTrackResults(frame, tracks);

        // ��ʾ������Ϣ
        std::string info = "Frame: " + std::to_string(frame_count) +
                           " | Detect: " + std::to_string(detect_time.count()) + "ms" +
                           " | Track: " + std::to_string(track_time.count()) + "ms" +
                           " | Tracks: " + std::to_string(tracks.size());
        cv::putText(frame, info, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(255, 255, 255), 1);

        // ���֡ʱ����Ϣ
        std::string time_info = "Time: " + std::to_string(static_cast<int>(current_frame_time_ms)) + "ms";
        cv::putText(frame, time_info, cv::Point(10, 90),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

        // д�������Ƶ
        writer.write(frame);

        // ������ʾ
        if (frame_count % 30 == 0)
        {
            std::cout << "����֡��: " << frame_count << std::endl;
        }
    }

    // �ͷ���Դ
    cap.release();
    writer.release();

    // ��ɼ�����д��ͳ����Ϣ
    counting_module.finishCounting(frame_count);

    // ��ʾͳ����Ϣ
    std::cout << "������ɣ�" << std::endl;
    std::cout << "��֡��: " << frame_count << std::endl;
    std::cout << "ƽ�����ʱ��: " << (total_detect_time / frame_count) << " ms/frame" << std::endl;
    std::cout << "ƽ��׷��ʱ��: " << (total_track_time / frame_count) << " ms/frame" << std::endl;
    std::cout << "��Խ����ߵ�Ŀ������: " << counting_module.getTotalCount() << std::endl;
    std::cout << "������浽: tracking_result_with_counting.mp4" << std::endl;
    std::cout << "������¼���浽: " << count_file << std::endl;
}

// ʵʱ����ͷ׷��ʾ��
void realtimeCameraTracking()
{
    std::cout << "\n=== ʵʱ����ͷ׷��ʾ�� ===" << std::endl;

    // ��ʼ���������׷����
    std::string engine_path = "./models/yolo11n.plan";
    YoloDetector detector(engine_path, 0, 0.45f, 0.25f, 80);
    TrackerModule tracker(30, 30, false, 0); // ׷����Ա

    // ������ͷ
    cv::VideoCapture cap(0);
    if (!cap.isOpened())
    {
        std::cout << "�޷�������ͷ��" << std::endl;
        return;
    }

    // ��������ͷ�ֱ���
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    std::cout << "�� 'q' ���˳�ʵʱ׷��" << std::endl;

    cv::Mat frame;
    while (true)
    {
        if (!cap.read(frame))
        {
            std::cout << "�޷���ȡ����ͷ���棡" << std::endl;
            break;
        }

        // ִ�м���׷��
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<Detection> detections = detector.inference(frame);
        std::vector<TrackResult> tracks = tracker.update(detections);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // ����׷�ٽ��
        TrackerModule::drawTrackResults(frame, tracks);

        // ��ʾ������Ϣ
        std::string fps_text = "FPS: " + std::to_string(1000.0 / duration.count());
        std::string track_text = "Tracks: " + std::to_string(tracks.size());
        cv::putText(frame, fps_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(0, 255, 0), 2);
        cv::putText(frame, track_text, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX,
                    0.7, cv::Scalar(0, 255, 0), 2);

        // ��ʾ����
        cv::imshow("ʵʱĿ��׷��", frame);

        // ����˳���
        if (cv::waitKey(1) & 0xFF == 'q')
        {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
}

int main()
{
    try
    {
        std::cout << "YOLO TensorRT Ŀ������׷�ٲ��Գ���" << std::endl;
        std::cout << "========================================" << std::endl;

        // ���ģ���ļ��Ƿ����
        if (!std::filesystem::exists("./models/yolo11n.plan"))
        {
            std::cout << "�����Ҳ���ģ���ļ� ./models/yolo11n.plan" << std::endl;
            std::cout << "��ȷ��ģ���ļ�����ȷ������modelsĿ¼��" << std::endl;
            return -1;
        }

        // ѡ��Ҫ���е�ʾ��
        int choice;
        std::cout << "��ѡ��Ҫ���е�ʾ����" << std::endl;
        std::cout << "1. ��Ŀ����" << std::endl;
        std::cout << "2. ��ƵĿ��׷�٣��������߼�����" << std::endl;
        std::cout << "3. ʵʱ����ͷ׷��" << std::endl;
        std::cout << "������ѡ�� (1-3): ";
        std::cin >> choice;

        switch (choice)
        {
        case 1:
            simpleDetectionExample();
            break;
        case 2:
            videoTrackingExample();
            break;
        case 3:
            realtimeCameraTracking();
            break;
        default:
            std::cout << "��Чѡ��" << std::endl;
            return -1;
        }
    }
    catch (const std::exception &e)
    {
        std::cout << "�������г���: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "\n����ִ����ɣ���������˳�..." << std::endl;
    std::cin.get();
    std::cin.get();
    return 0;
}