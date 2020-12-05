import processing.serial.*;

Serial myPort;

//SIGNAL VARIABLES
ArrayList<Circle> list_of_circles = new ArrayList<Circle>();
ArrayList<Line> list_of_lines = new ArrayList<Line>();

int maxSignalExtent = 500;
int signalGrowthSpeed = 2;

int globalThreshold = 30;

boolean serial_available = true;
String printstr = "";
String print_topo = "";
String incoming_millis_str = "";
int[] topo = new int[16];

Score score = new Score();
Instrument snare = new Instrument("SNARE");
Instrument kick = new Instrument("KICK");
Instrument hihat = new Instrument("HIHAT");
Instrument standtom1 = new Instrument("STANDTOM 1");
Instrument tom2 = new Instrument("TOM 2");
Instrument ride = new Instrument("RIDE");
Instrument cowbell = new Instrument("COWBELL");
Instrument crash1 = new Instrument("CRASH 1");

Instrument list_of_instruments[] = {snare, kick, hihat, standtom1, tom2, ride, cowbell, crash1};

Grid grid;


///////////////////////////////////////////////////////////////
////////////////////////// SETUP //////////////////////////////
///////////////////////////////////////////////////////////////


void setup()
{

        size(1600, 800, FX2D);
        printArray(Serial.list());
        String portName = Serial.list()[0]; // find right Serial port from list
        try {
                myPort = new Serial(this, portName, 115200);
        } catch(Exception e) {
                println(e);
                serial_available = false;
        }

        // read_json_from_file();

        score.beat_plot.drawMode = score.beat_plot.BARPLOT;
        score.beat_plot.set_ticks(1, 3);
        score.beat_plot.set_title("BEAT SUM");


        for (int i = 0; i<list_of_instruments.length; i++)
        {
                Instrument instr = list_of_instruments[i];

                // stroke plot:
                instr.plot.drawMode = instr.plot.BARPLOT;
                instr.plot.set_ticks(16,2);
                instr.plot.set_title(instr.title);
                colorMode(HSB);
                instr.plot.pointsColor = color(i * (255/list_of_instruments.length), 100, 100);

                // cc_plot:
                instr.cc_plot.drawMode = instr.plot.BARPLOT;
                instr.cc_plot.set_ticks(200,1);
                instr.cc_plot.strokeColor = color(i * (255/list_of_instruments.length), 100, 100);
                instr.cc_plot.draw_title = false;
                instr.cc_plot.draw_labels = false;
                instr.cc_plot.draw_xticks = false;
                // instr.cc_plot.draw_yticks = false;
                instr.cc_plot.pointsColor = color(i * (255/list_of_instruments.length), 100, 100);

        }
        grid = new Grid(width/2, 0);
}

///////////////////////////////////////////////////////////////
////////////////////////// LOOP ///////////////////////////////
///////////////////////////////////////////////////////////////

void draw()
{
        background(0);
        // ---------------------- DEBUG CODE ------------------------
        /*
           background(a%255, b%100, c%50);
           fill(255-c, 255-a, 255-b, ((a+b+c)/30)%100);
           ellipse(height/2, width/2, g++%width, g%height);
           //println(b);
         */

        // initialize grid once:
        if (snare.topo.size() == 16 && grid.init)
        {
                grid.create_vertices(16, 16);
                // grid.init = false;
        }

        // draw vertices:
        if (snare.topo.size() == 16) grid.draw();

        // ------------------------ draw plots ---------------------------
        int dist_left = 50;
        int dist_top = 30;

        if (json_was_initialized)
        {
                score.beat_plot.updateValues(score.topo, true);
                score.beat_plot.draw(dist_left, 650);
        }

        if (json_was_initialized)
        {
                for (int i = 0; i<list_of_instruments.length; i++)
                {
                        Instrument instr = list_of_instruments[i];

                        instr.plot.updateValues(instr.topo, true);
                        if (json_was_initialized) instr.plot.draw(dist_left, dist_top+ i* (height-200)/list_of_instruments.length);

                        if (json_was_initialized) instr.cc_plot.draw(dist_left + 350, dist_top+ i* (height-200)/list_of_instruments.length);

                        textAlign(LEFT,LEFT);
                        textSize(14);
                        colorMode(HSB);
                        fill(instr.cc_plot.pointsColor);
                        text(instr.title + ": "
                             + instr.effect + "\n"
                             + "ø:\t" + instr.average_smooth
                             + "/" + instr.activation_thresh
                             + "\nCC:\t" + instr.cc_val
                             + " | " + instr.cc_increase
                             + " | " + instr.cc_decay,
                             instr.plot.axis_width + instr.cc_plot.axis_width + dist_left*2, dist_top + i* (height-200)/list_of_instruments.length);

                        instr.record_value(instr.cc_val, 200);
                }
        }

        // ------------------------ Area for Strokes -----------------------
        stroke(180);
        noFill();
        rect(400, 600, 785, height-10);
        // draw strokes:
        for (int i = 0; i<list_of_instruments.length; i++)
        {
                if (list_of_instruments[i].wasHit)
                {
                        list_of_instruments[i].record_String("※", (height-20-600)/4);
                        list_of_instruments[i].wasHit = false;
                }

                list_of_instruments[i].draw_strings(400+i*50, 4);
                list_of_instruments[i].record_String(" ", (height-600-20)/4);
        }


        // ------------------------ Auxiliary Info -----------------------

        // mouse position and circles
        textAlign(RIGHT, BOTTOM);
        textSize(14);
        fill(255);
        text(score.step, width, height);

        // millis:
        textAlign(LEFT, BOTTOM);
        text(score.elapsedMillis, 0, height);
        // ------------------------ Read Serial ---------------------------
        serialEvent(myPort);

        // ------------------------ print beat step -------------------
        textAlign(CENTER,BOTTOM);
        // int current_16th_step = current_16ths_step();
        if (score.current_beat_pos % 4 == 0)
        {
                textSize(24);
                colorMode(RGB);
                fill(255,0,0);
        }
        else
        {
                textSize(16);
                fill(255);
        }
        text(str(score.current_beat_pos), width/2, height);

        // --------------------- print incoming millis -------------------
        textAlign(LEFT,BOTTOM);
        textSize(14);
        fill(255);
        text(incoming_millis_str, 0, height);
}
