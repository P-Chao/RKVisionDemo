#include "yolov10_wrap.h"
#include "common.h"

int AlgYolo10::Init(const char *model_path) {
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    int ret = init_yolov10_model(model_path);
    if (ret != 0) {
        printf("init_yolov10_model fail! ret=%d model_path=%s\n", ret, model_path);
        return -1;
    }
    init_post_process();
    return 0;
}

int AlgYolo10::Deinit() {
    deinit_post_process();

    int ret = release_yolov10_model(&rknn_app_ctx);
    if (ret != 0)
    {
        printf("release_yolov10_model fail! ret=%d\n", ret);
    }

    if (src_image.virt_addr != NULL)
    {
        free(src_image.virt_addr);
    }
    return 0;
}

int AlgYolo10::Detect(const cv::Mat& image) {
    image_buffer_t src_image;
    memset(&src_image, 0, sizeof(image_buffer_t));

    ret = read_image(image_path, &src_image);

    if (ret != 0) {
        printf("read image fail! ret=%d image_path=%s\n", ret, image_path);
        return -1;
    }

    object_detect_result_list od_results;

    ret = inference_yolov10_model(&rknn_app_ctx, &src_image, &od_results);
    if (ret != 0)
    {
        printf("inferance_yolov10_model fail! ret=%d\n", ret);
        goto out;
    }

    // draw box and prob
    char text[256];
    for (int i = 0; i < od_results.count; i++)
    {
        object_detect_result *det_result = &(od_results.results[i]);
        printf("%s @ (%d %d %d %d) %.3f\n", coco_cls_to_name(det_result->cls_id),
               det_result->box.left, det_result->box.top,
               det_result->box.right, det_result->box.bottom,
               det_result->prop);
        int x1 = det_result->box.left;
        int y1 = det_result->box.top;
        int x2 = det_result->box.right;
        int y2 = det_result->box.bottom;

        draw_rectangle(&src_image, x1, y1, x2 - x1, y2 - y1, COLOR_BLUE, 3);

        sprintf(text, "%s %.1f%%", coco_cls_to_name(det_result->cls_id), det_result->prop * 100);
        draw_text(&src_image, text, x1, y1 - 20, COLOR_RED, 10);
    }

    write_image("out.png", &src_image);


}



