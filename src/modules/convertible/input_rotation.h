//
// Created by raffaele on 18/12/19.
//

#ifndef E_GADGET_CONVERTIBLE_INPUT_ROTATION_H
#define E_GADGET_CONVERTIBLE_INPUT_ROTATION_H

typedef struct _TransformationMatrix {
   float values[9];
} TransformationMatrix;


static const float MATRIX_ROTATION_IDENTITY[6] = {1, 0, 0, 0, 1, 0 };
static const float MATRIX_ROTATION_270[6] = {0, 1, 0, -1, 0, 1 };
static const float MATRIX_ROTATION_180[6] = {-1, 0, 1, 0, -1, 1 };
static const float MATRIX_ROTATION_90[6] = {0, -1, 1, 1, 0, 0 };

// "Coordinate Transformation Matrix"
static const char *CTM_name = "Coordinate Transformation Matrix";
#endif //E_GADGET_CONVERTIBLE_INPUT_ROTATION_H
