(= GRAVITY  0.2
   FRICTION 0.9
)

(= collide? (func (x0 y0 s0 x1 y1 s1)
  (and
    (< x1 (+ x0 s0)) (< x0 (+ x1 s1))
    (< y1 (+ y0 s0)) (< y0 (+ y1 s1))
  )
))

(= distance (func (a b)
  (let x0 (+ (a':get-x) (/ (a':get-size) 2))
       y0 (+ (a':get-y) (/ (a':get-size) 2))
       x1 (+ (b':get-x) (/ (b':get-size) 2))
       y1 (+ (b':get-y) (/ (b':get-size) 2))
  )
  (sqrt (+ (square (- x1 x0)) (square (- y1 y0))))
))

(= Actor (class
  :float nil  

  :x nil
  :get-x (func () :x)
  
  :y nil
  :get-y (func () :y)

  :size nil
  :get-size (func () :size)

  :dx 0
  :get-dx (func () :dx)

  :dy 0
  :get-dy (func () :dy)

  :move (func (x y)
    (= :dx (+ :dx x)
       :dy (+ :dy y)
    )
  )
  
  :on-ground nil
  :on-ground? (func () :on-ground)

  :smack nil
  :smack? (func () :smack)
  
  :bounced nil
  :step (func ()
    (= :bounced nil)
    (if (not :float) (= :dy (+ :dy GRAVITY)))
    ; update velocity
    (= :dx (* :dx FRICTION)
       :dy (* :dy FRICTION)
    )
    ; update collision with tilemap
    (if (not :float) (do
      ; build AABB
      (let x1 (+ :x (- :size 1))
           y1 (+ :y (- :size 1))
           x0 (+ :x 1)
           y0 (+ :y 1)
      )
      ; move on x-axis
      (= :smack (or
        (solid? (+ :dx (if (< :dx 0) x0 x1)) y0)
        (solid? (+ :dx (if (< :dx 0) x0 x1)) y1)
      ))
      (if :smack (= :dx (* :dx -0.5)))
      ; move on y-axis
      (= :on-ground (or
        (solid? x0 (+ :dy (if (< :dy 0) y0 y1)))
        (solid? x1 (+ :dy (if (< :dy 0) y0 y1)))
      ))
      (if :on-ground (do
        (if (< :dy 0) (= :on-ground nil))
        (= :dy 0) 
      ))
    ))
    ; update position
    (= :x (+ :x :dx)
       :y (+ :y :dy)
    )
  )

  init (func (x y size float)
    (= :x x
       :y y
       :size size
       :float float
    )
  )

  :bounce (func ()
    (if (not :bounced) (do
      (= :jump 0
         :bounced t
         :dy (* :dy -1.1)
      )
    ))
  )

  :alive t
  :kill (func (alive) (= :alive alive))
  :alive? (func () :alive)
))
