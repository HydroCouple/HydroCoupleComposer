/*!
 * \file   testcomponent.cpp
 * \brief  A minimal, real HydroCouple component library used to verify the
 *         Composer's loader end to end.
 *
 * Deliberately built from the SDK's own AbstractModelComponent rather than a
 * hand-rolled stub: the loader must be proven against the same base classes
 * real components use, including the vtable layout that crosses the library
 * boundary.
 */

#include "plugins/componentabi.h"

#include "hydrocouplesdk/component/abstractmodelcomponent.h"
#include "hydrocouplesdk/component/abstractmodelcomponentinfo.h"
#include "hydrocouplesdk/core/dimension.h"
#include "hydrocouplesdk/core/valuedefinition.h"
#include "hydrocouplesdk/data/argument1d.h"
#include "hydrocouplesdk/data/argument2d.h"
#include "hydrocouplesdk/data/exchangeitems1d.h"

#include <memory>
#include <string>
#include <vector>

using namespace HydroCouple;
using namespace HydroCouple::SDK;

namespace
{
  constexpr int kCellCount = 3;
  constexpr int kTimeSteps = 400;

  /*!
   * \brief A component that publishes one argument and one output.
   */
  class TestComponent : public AbstractModelComponent
  {
    public:
      explicit TestComponent(std::string_view id)
        : AbstractModelComponent(id, "Loader Test Component")
      {
      }

      std::vector<std::string> validate() override
      {
        setStatus(ComponentStatus::Validating);
        setStatus(ComponentStatus::Valid);
        return {};
      }

      void prepare() override
      {
        setStatus(ComponentStatus::Preparing);
        m_step = 0;
        setStatus(ComponentStatus::Updated, "prepared");
      }

      void update(const std::vector<IOutput *> & = {}) override
      {
        setStatus(ComponentStatus::Updating);

        for (int cell = 0; cell < kCellCount; ++cell)
        {
          (*values)[cell] = static_cast<double>(m_step + cell);
        }

        ++m_step;

        // Long enough that a run can be observed mid-flight — a two-step run
        // would finish before a pause request could ever be honoured, which
        // would make the pause tests vacuous.
        setStatus(m_step >= kTimeSteps ? ComponentStatus::Done
                                       : ComponentStatus::Updated,
                  "stepped");
      }

      void finish() override
      {
        setStatus(ComponentStatus::Finishing);
        setStatus(ComponentStatus::Finished);
      }

      Argument1DDouble *scale = nullptr;
      Argument1DInt *iterations = nullptr;
      Argument1DString *label = nullptr;
      Argument1DString *regime = nullptr;
      Argument2DDouble *grid = nullptr;
      Input1DDouble *inflow = nullptr;
      Output1DDouble *values = nullptr;

    protected:
      void createArguments() override
      {
        m_quantity.reset(Quantity::unitLess("Scale"));

        scale = new Argument1DDouble("scale", &m_dimension, kCellCount,
                                     m_quantity.get(), this);
        addArgument(scale);

        // A scalar integer — the simplest spin-box case.
        m_count.reset(Quantity::unitLess("Iterations"));
        iterations = new Argument1DInt("iterations", &m_scalarDimension, 1,
                                       m_count.get(), this);
        addArgument(iterations);

        // A free-text scalar.
        m_text.reset(Quantity::unitLess("Label"));
        label = new Argument1DString("label", &m_scalarDimension, 1,
                                     m_text.get(), this);
        addArgument(label);

        // A categorical argument: its value definition enumerates the
        // choices, which is what turns an editor into a combo box.
        m_regime = std::make_unique<Quality>(
          "Regime", std::vector<std::string>{"steady", "dynamic", "kinematic"});
        regime = new Argument1DString("regime", &m_scalarDimension, 1,
                                      m_regime.get(), this);
        addArgument(regime);

        // A rank-2 argument — the table case.
        m_gridQuantity.reset(Quantity::unitLess("Grid"));
        grid = new Argument2DDouble("grid", &m_rowDimension, &m_columnDimension,
                                    2, 3, m_gridQuantity.get(), this);
        addArgument(grid);
      }

      bool initializeArguments(std::string &message) override
      {
        message.clear();
        return true;
      }

      void createInputs() override
      {
        inflow = new Input1DDouble("inflow", &m_dimension, kCellCount,
                                   m_quantity.get(), this);
        addInput(inflow);
      }

      void createOutputs() override
      {
        values = new Output1DDouble("values", &m_dimension, kCellCount,
                                    m_quantity.get(), this);
        addOutput(values);
      }

      void initializeFailureCleanUp() override {}

    private:
      Dimension m_dimension{"cell", "Cell index"};
      Dimension m_scalarDimension{"scalar", "Single value"};
      Dimension m_rowDimension{"row", "Row index"};
      Dimension m_columnDimension{"column", "Column index"};
      std::unique_ptr<Quantity> m_quantity;
      std::unique_ptr<Quantity> m_count;
      std::unique_ptr<Quantity> m_text;
      std::unique_ptr<Quantity> m_gridQuantity;
      std::unique_ptr<Quality> m_regime;
      int m_step = 0;
  };

  /*!
   * \brief Metadata for TestComponent.
   */
  class TestComponentInfo : public AbstractModelComponentInfo
  {
    public:
      TestComponentInfo()
        : AbstractModelComponentInfo("composer.test.component")
      {
        setCaption("Loader Test Component");
        setDescription("Minimal component used by the Composer loader tests.");
        setDeveloper("HydroCouple");
        setVersion("1.0.0");
        setTags({"Test", "Loader"});
      }

      std::unique_ptr<IModelComponent> createComponentInstance() override
      {
        return std::make_unique<TestComponent>("composer.test.instance");
      }
  };
} // namespace

HYDROCOUPLE_DECLARE_COMPONENT(TestComponentInfo)
