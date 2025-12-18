/*
<컴파일>

<사용법>

*/
#include "lepton.h"

int main(int argc, char *argv[]){   

    if (argc == 1)
    {
        printf("인수를 입력하시오 -- 1: 이미지 표시 | 2: 이미지 저장 | 3: 영상 스트리밍\n");
        return -1;
    }
    else if (argc > 2)
    {
        printf("인수 개수는 1개.\n");
        return -1;
    }

    int fd = init_lepton();
    if (fd < 0)
    {
        printf("Error while initializing Lepton\n");
        return -1;
    }

    int ret = 0;
    switch(argv[1][0])
    {
        case '1':
            ret = visualize_img(fd);
            if (ret < 0)
                printf("visualize_img() 오류 발생\n");
            break;
        case '2':
            ret = save_img(fd);
            if (ret < 0)
                printf("save_img() 오류 발생\n");
            break;
        case '3':
            ret = lepton_stream(fd);
            if (ret < 0)
                printf("lepton_stream() 오류 발생\n");
            break;
        default:
            printf("잘못된 인수입니다. 1, 2, 3 중 하나를 입력하세요.\n");
            break;
    }

    close(fd);
    return (ret < 0) ? -1 : 0;
}