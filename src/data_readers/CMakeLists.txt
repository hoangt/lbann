add_subdirectory(patchworks)

add_sources(data_reader.cpp
            data_reader_mnist.cpp
            data_reader_imagenet.cpp
            data_reader_imagenet_single.cpp
#            data_reader_imagenet_cv.cpp
#            data_reader_imagenet_single_cv.cpp
            data_reader_nci.cpp
            data_reader_cifar10.cpp
            image_utils.cpp
            image_preprocessor.cpp
            cv_augmenter.cpp
            cv_colorizer.cpp
            cv_normalizer.cpp
            cv_process.cpp
            cv_process_patches.cpp
            cv_resizer.cpp
            cv_utils.cpp
            data_reader_numpy.cpp
            data_reader_synthetic.cpp
            data_reader_csv.cpp
)
