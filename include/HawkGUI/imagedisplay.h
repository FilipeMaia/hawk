#ifndef _IMAGE_DISPLAY_H_
#define _IMAGE_DISPLAY_H_ 1

#include <QFrame>
#include <QFileInfo>

class GeometryControl;
class ImageView;

class QGridLayout;
class QFocusFrame;
class OutputWatcher;
class ImageCategory;
class ImageFrame;
class ProcessControl;

class ImageDisplay : public QFrame
{
  Q_OBJECT
  public:
  ImageDisplay(QWidget * parent);
  void setImageCategories(QList<ImageCategory *> * ic);
  OutputWatcher * getOutputWatcher();
  bool isProcessRunning();
  ProcessControl * getProcess();
public slots:
  void onFocusedViewChanged(ImageView * focused);
  void setLockedTransformation(bool locked);
  void setLockedBrowse(bool locked);
  void translateView(QPointF ammount);
  void scaleView(qreal scale);
  void onProcessStarted(QString type,QString path,ProcessControl * process);
  void onProcessStopped();
  void updateLatestOutput(QString type,QFileInfo file,QFileInfo old);
  void setAutoUpdate(bool update);
  void loadUserSelectedImage();
  void loadInitialProcessOutput(QString key, QFileInfo file);
  void shiftSelectedImage();
  void displayAmplitudes();
  void displayPhases();
  void displayMask();
  void setDisplay(int);
  void setColorGray();
  void setColorJet();
  void setColorHot();
  void setColorWheel();
  void setColorTraditional();
  void setColorRainbow();
  void setColormap(int color);
  void maxContrastSelectedImage();
  void logScaleSelectedImage(bool on);
 signals:
  void focusedViewChanged(ImageView * focused);
  private slots:
  void onImageLoaded(QString image);
private:
  void initImageViewers();
  GeometryControl * geometryControl;
  QList <ImageView *> imageViewers;
  QList <ImageFrame *> imageFrames;
  QList <QPoint> imageViewersPos;
  QGridLayout * gridLayout;
  QFocusFrame * focusFrame;
  ImageView * selected;
  bool locked;
  OutputWatcher * outputWatcher;
  QList<ImageCategory *> * imageCategories;
  bool processRunning;  
  ProcessControl * process;
  bool lockedBrowse;
};
#endif