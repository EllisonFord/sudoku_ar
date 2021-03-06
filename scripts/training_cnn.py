from __future__ import print_function
import keras
from keras.models import Sequential
from keras.layers import Dense, Dropout, Flatten
from keras.layers import Conv2D, MaxPooling2D
from keras import backend as K
from preparing_dataset import *
from params import *
from time import strftime


# IMPORTANT, CHOOSE between 'combination', 'mnist' or 'char74k'
chosen_dataset = 'char74k'
remove_char = 0
train_evenly = False  # Choose this if you are training on combination and you want mnist and char74k to have the same num_samples


# the data, split between train and test sets
(x_train, y_train), (x_test, y_test) = load_our_dataset(dataset=chosen_dataset,
                                                        whiten=remove_char,
                                                        even_training=train_evenly)

# Uncomment below if you would like to see what the net is going to train on.
# see_samples(x_train, y_train, nSamples=20)


if K.image_data_format() == 'channels_first':
    x_train = x_train.reshape(x_train.shape[0], 1, img_rows, img_cols)
    x_test = x_test.reshape(x_test.shape[0], 1, img_rows, img_cols)
    input_shape = (1, img_rows, img_cols)
else:
    x_train = x_train.reshape(x_train.shape[0], img_rows, img_cols, 1)
    x_test = x_test.reshape(x_test.shape[0], img_rows, img_cols, 1)
    input_shape = (img_rows, img_cols, 1)

# Make sure that all of the bits will be present
x_train = x_train.astype('float32')
x_test = x_test.astype('float32')

# Normalise
x_train /= 255
x_test /= 255

# convert class vectors to binary class matrices
y_train = keras.utils.np_utils.to_categorical(y_train, num_classes)

y_test = keras.utils.np_utils.to_categorical(y_test, num_classes)


model = Sequential()

model.add(Conv2D(32,
                 kernel_size=(3, 3),
                 activation='relu',
                 input_shape=input_shape))

model.add(Conv2D(64, (3, 3), activation='relu'))
model.add(MaxPooling2D(pool_size=(2, 2)))
model.add(Dropout(0.25))
model.add(Flatten())
model.add(Dense(128, activation='relu'))
model.add(Dropout(0.5))
model.add(Dense(num_classes, activation='softmax'))

print(model.summary())


model.compile(loss=keras.losses.categorical_crossentropy,
              optimizer=keras.optimizers.Adadelta(),
              metrics=['accuracy'])

model.fit(x_train,
          y_train,
          batch_size=batch_size,
          epochs=epochs,
          verbose=1,
          validation_data=(x_test, y_test))

score = model.evaluate(x_test,
                       y_test,
                       verbose=False)

test_loss = 'Test loss:', score[0]
test_acc = 'Test accuracy:', score[1]
print(test_loss)
print(test_acc)

time_stamp = strftime("%Y-%m-%d_%H.%M.%S")

#net_details = []
#net_details.append()

save_model(model,
           title_dataset=chosen_dataset,
           time=time_stamp,
           net_details=None)
