#ifndef HYDROCOUPLEPROJECT_H
#define HYDROCOUPLEPROJECT_H

#include <QObject>
#include <QFileInfo>

class GModelComponent;
class GModelComponentConnection;

class HydroCoupleProject : public QObject
{
      friend class GModelComponent;

      Q_OBJECT
      Q_PROPERTY(QFileInfo ProjectFile READ projectFile)
      Q_PROPERTY(QList<GModelComponent*> ModelComponents READ modelComponents)

   public:
      HydroCoupleProject(QObject *parent);

      virtual ~HydroCoupleProject();

      //!File to save project in
      QFileInfo projectFile() const;

      //!Instantiated model components
      QList<GModelComponent*> modelComponents() const;

      //!add instantiated component to list of components
      void addComponent(GModelComponent *component);

      //!remove component from list of instantiated components
      bool deleteComponent(GModelComponent *component);

      /*!
       * \brief hasChanges
       * \return
       */
      bool hasChanges() const;

      /*!
       * \brief readProjectFile
       * \param file
       * \return
       */
      static HydroCoupleProject* readProjectFile(const QFileInfo& file);

   signals:
      //! emit when component is added to update ui and graphics
      void componentAdded(GModelComponent *component);

      /*!
       * \brief emit when component is removed to update UI and graphics.
       */
      void componentDeleting(GModelComponent *component);

      /*!
       * \brief stateModified
       * \param hasChanges
       */
      void stateModified(bool hasChanges);


      void postMessage(const QString& message);


      void setProgress(bool visible, const QString& message, int progressvalue , int min = 0, int max = 100);

   public slots:

      /*!
       * \brief onTriggerChanged
       * \param triggerModelComponent
       */
      void onTriggerChanged(GModelComponent *triggerModelComponent);

      /*!
       * \brief onSaveProject
       */
      void onSaveProject();

      /*!
       * \brief onSaveProjectAs
       * \param file
       */
      void onSaveProjectAs(const QFileInfo& file);

   private:
      //!Performs deep check to see if component has already been added
      bool contains(GModelComponent *component) const;

      /*!
       * \brief setHasChanges
       * \param hasChanges
       */
      void setHasChanges(bool hasChanges);

   private:
      QList<GModelComponent*> m_modelComponents;
      QFileInfo m_projectFile;
      bool m_hasChanges;
};

Q_DECLARE_METATYPE(HydroCoupleProject*)

#endif // HYDROCOUPLEPROJECT_H
